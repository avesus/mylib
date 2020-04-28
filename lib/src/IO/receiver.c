#include <IO/receiver.h>

void
prepare_send_recvr(receiver_t * recvr, HEADER_TYPE hdr, uint8_t * data) {


    DBG_ASSERT(hdr > HEADER_SIZE,
               "Error trying to prepare an empty message...\n");

    PRINT(HIGH_VERBOSE, "prep_writing[%p, %d, %d]\n", data, hdr, data[0]);
    store_recvr_outbuf(recvr, (uint8_t *)(&hdr), HEADER_SIZE, ACQUIRE);
    store_recvr_outbuf(recvr,
                       (uint8_t *)data,
                       (hdr - (HEADER_SIZE)),
                       RELEASE);

    PRINT(HIGH_VERBOSE, "Writing Type: %d\n", OUTBUF_PTR(recvr->outbuf)[HEADER_SIZE]);
    
    reset_recvr_event(recvr, &handle_event, EV_WRITE, WRITING);
}
    
void
clear_recvr_outbuf(receiver_t * recvr) {
    ACQUIRE_OUTBUF(recvr->outbuf);
    recvr->outbuf_cur_size = 0;
    RELEASE_OUTBUF(recvr->outbuf);
}

void
store_recvr_outbuf(receiver_t * recvr,
                   uint8_t *    to_add,
                   uint32_t     size_to_add,
                   uint32_t     lock_flags) {

    if (lock_flags & ACQUIRE) {
        ACQUIRE_OUTBUF(recvr->outbuf);
    }
    if (OUTBUF_PTR(recvr->outbuf) == NULL) {
        DBG_ASSERT(recvr->outbuf_cur_size == 0,
                   "error null outbuf with non 0 size\n");

        uint8_t * new_ptr = (uint8_t *)mycalloc(size_to_add, sizeof(uint8_t));
        memcpy(new_ptr, to_add, size_to_add);
        recvr->outbuf_size     = size_to_add;
        recvr->outbuf_cur_size = size_to_add;

        if (lock_flags & RELEASE) {
            // this releases
            recvr->outbuf = new_ptr;
        }
        else {
            ACQUIRE_OUTBUF(new_ptr);
            recvr->outbuf = new_ptr;
        }
    }
    else if (recvr->outbuf_size < recvr->outbuf_cur_size + size_to_add) {
        const uint32_t _new_size =
            MAX(recvr->outbuf_cur_size + size_to_add, 2 * recvr->outbuf_size);

        uint8_t * new_ptr = (uint8_t *)mycalloc(_new_size, sizeof(uint8_t));
        memcpy(new_ptr, OUTBUF_PTR(recvr->outbuf), recvr->outbuf_cur_size);
        memcpy(new_ptr + recvr->outbuf_cur_size, to_add, size_to_add);

        recvr->outbuf_cur_size += size_to_add;
        recvr->outbuf_size = _new_size;

        if (lock_flags & RELEASE) {
            // this releases
            recvr->outbuf = new_ptr;
        }
        else {
            ACQUIRE_OUTBUF(new_ptr);
            recvr->outbuf = new_ptr;
        }
    }
    else {

        memcpy(OUTBUF_PTR(recvr->outbuf) + recvr->outbuf_cur_size,
               to_add,
               size_to_add);

        recvr->outbuf_cur_size += size_to_add;
        if (lock_flags & RELEASE) {
            RELEASE_OUTBUF(recvr->outbuf);
        }
    }
    PRINT(HIGH_VERBOSE,
          "New outbuf stored [%p (%d/%d)]\n",
          recvr->outbuf,
          recvr->outbuf_cur_size,
          recvr->outbuf_size);
}


void
free_recvr(receiver_t * recvr) {
    PRINT(HIGH_VERBOSE, "Start freeing reciever: %d\n", recvr->my_recvr_idx);
    if (event_del(&(recvr->recvr_ev)) == -1) {
        errdie("Couldn't delete receiver event\n");
    }

    close(recvr->fd);
    myfree(recvr->buf);
    myfree(OUTBUF_PTR(recvr->outbuf));
    if (recvr->dest) {
        recvr->dest(recvr);
    }
    PRINT(HIGH_VERBOSE, "Done freeing reciever: %d\n", recvr->my_recvr_idx);
    myfree(recvr);
}

receiver_t *
init_stdin_recvr(uint32_t            init_buf_size,
                 void *              parent,
                 struct event_base * base,
                 cmd_handler         rd_handle) {
    receiver_t * stdin_recvr =
        init_recvr(STDIN_FILENO, init_buf_size, 0, parent, base, rd_handle);

    make_blocking(stdin_recvr->fd);
    stdin_recvr->ev_state = HANDLING;
    stdin_recvr->rd_state = READING_NONE;
    reset_recvr_event(stdin_recvr, &handle_stdin_event, EV_READ, WAITING);

    return stdin_recvr;
}

receiver_t *
init_recvr(int32_t             fd,
           uint32_t            init_buf_size,
           uint32_t            recvr_idx,
           void *              parent,
           struct event_base * base,
           cmd_handler         handle) {

    receiver_t * new_recvr = (receiver_t *)mycalloc(1, sizeof(receiver_t));

    make_nonblock(fd);
    new_recvr->fd           = fd;
    new_recvr->my_recvr_idx = recvr_idx;

    new_recvr->buf_size = MAX(init_buf_size, MIN_BUF_SIZE);
    new_recvr->buf      = (uint8_t *)mycalloc(init_buf_size, sizeof(uint8_t));

    new_recvr->my_parent = (void *)parent;
    new_recvr->my_base   = base;

    new_recvr->rd_handle = handle;

    event_set(&(new_recvr->recvr_ev),
              fd,
              EV_READ,
              handle_event,
              (void *)new_recvr);
    event_base_set(base, &(new_recvr->recvr_ev));
    if (event_add(&(new_recvr->recvr_ev), NULL) == (-1)) {
        errdie("Unable to add new recvr event\n");
    }

    NEW_FRAME(FMTS("%p", "%p", "%d", "%d", "%d"),
              VARS(new_recvr,
                   new_recvr->buf,
                   fd,
                   init_buf_size,
                   new_recvr->buf_size));


    return new_recvr;
}


void
reset_recvr_event(receiver_t * recvr,
                  void         ev_handler(const int, const short, void *),
                  int32_t      new_flags,
                  event_states ev_state) {

    if (event_del(&(recvr->recvr_ev)) == -1) {
        errdie("Couldn't reset receiver event\n");
    }

    PRINT(HIGH_VERBOSE,
          "Recvr[%d, %d] States %d -> %d\n",
          recvr->my_recvr_idx,
          recvr->fd,
          recvr->ev_state,
          ev_state);
    recvr->ev_state = ev_state;
    event_set(&(recvr->recvr_ev),
              recvr->fd,
              new_flags,
              ev_handler,
              (void *)recvr);
    event_base_set(recvr->my_base, &(recvr->recvr_ev));
    if (event_add(&(recvr->recvr_ev), NULL) == (-1)) {
        errdie("Unable to readd recvr event\n");
    }
}


void
handle_stdin_event(const int fd, const short which, void * arg) {
    receiver_t * stdin_recvr = (receiver_t *)arg;

    DBG_ASSERT(stdin_recvr->fd == STDIN_FILENO,
               "Error: non-stdin receiver in stdin handler!\n\t"
               "idx    : %d\n\t"
               "fds    : %d vs %d\n",
               stdin_recvr->my_recvr_idx,
               stdin_recvr->fd,
               STDIN_FILENO);
    reset_recvr_event(stdin_recvr, &handle_stdin_event, EV_READ, WAITING);
    stdin_recvr->rd_state = READING_BODY;
    char ch;
    while (1) {
        int32_t ret;
        ret = read(stdin_recvr->fd, &ch, sizeof(char));
        if (ret == (-1)) {
            if (errno == EAGAIN) {
                continue;
            }
            errdie("Failed to read stdin\n");
        }
        else if (ret == 0) {
            break;
        }
        else {
            stdin_recvr->buf[stdin_recvr->amt_read++] = ch;

            if ((stdin_recvr->amt_read + 1) == stdin_recvr->buf_size) {
                stdin_recvr->buf_size = 2 * stdin_recvr->buf_size;
                stdin_recvr->buf =
                    (uint8_t *)realloc(stdin_recvr->buf, stdin_recvr->buf_size);

                if (stdin_recvr->buf == NULL) {
                    errdie("Realloc of buf failed\n");
                }

                memset(stdin_recvr->buf + (stdin_recvr->buf_size / 2),
                       0,
                       stdin_recvr->buf_size / 2);
            }
            if (ch == 0 || ch == '\n') {
                break;
            }
            DBG_ASSERT(stdin_recvr->buf_size > stdin_recvr->amt_read,
                       "Error: buf_size(%d) < amt_read(%d) ????\n",
                       stdin_recvr->buf_size,
                       stdin_recvr->amt_read);
        }
    }

    if (ch == 0 || ch == '\n') {
        stdin_recvr->rd_handle(stdin_recvr, NULL);
    }
}


void
handle_event(const int fd, const short which, void * arg) {
    receiver_t * recvr = (receiver_t *)arg;
    PRINT(HIGH_VERBOSE,
          "In handle_event(%d, %d)\n\t"
          "fd   : %d\n\t"
          "idx  : %d\n",
          recvr->ev_state,
          recvr->rd_state,
          recvr->fd,
          recvr->my_recvr_idx);

    if (recvr->ev_state == WAITING) {
        recvr->ev_state = READING;
    }
    if (recvr->ev_state == READING) {
        io_data * ret_data = handle_read(recvr);
        if (ret_data != NULL) {
            PRINT(HIGH_VERBOSE,
                  "Recvr[%d] got data {\n\t"
                  "length   : %d\n\t"
                  "data     : %s\n\t"
                  "}\n",
                  recvr->fd,
                  ret_data->length,
                  ret_data->data);
            recvr->ev_state = HANDLING;
            reset_recvr_event(recvr, &handle_event, EV_READ, HANDLING);
            recvr->rd_handle((void *)recvr, ret_data);
            if (recvr->ev_state == HANDLING) {
                // if was set to writing do nothing...
                recvr->ev_state = WAITING;
            }
        }
    }
    else if (recvr->ev_state == HANDLING) {
        // do nothing for now
    }
    else if (recvr->ev_state == WRITING) {
        handle_write(recvr);
    }
    else {
        die("Receiver in invalid ev_state: %d\n", recvr->rd_state);
    }
    return;
}

static void
reset_recvr_rd_state(receiver_t * recvr) {
    DBG_ASSERT(recvr->amt_read == recvr->amt_expected,
               "Premature reset\n\t"
               "fd      : %d\n\t"
               "read    : %d\n\t"
               "expec   : %d\n",
               recvr->fd,
               recvr->amt_read,
               recvr->amt_expected);

    recvr->amt_read     = 0;
    recvr->amt_expected = 0;
    recvr->rd_state     = READING_NONE;
}


static void
do_read(receiver_t * recvr) {

    int32_t ret = read(recvr->fd,
                       recvr->buf + recvr->amt_read,
                       recvr->amt_expected - recvr->amt_read);

    PRINT(HIGH_VERBOSE,
          "do_read(%d) -> %d - %d -> %d / %d\n",
          recvr->fd,
          recvr->amt_expected,
          recvr->amt_read,
          recvr->amt_expected - recvr->amt_read,
          ret);

    // if EAGAIN just do nothing. fd's will likely be non blocking so this
    // really should never matter
    if (ret == (-1)) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            errdie("Failed to read from fd %d\n", recvr->fd);
        }
    }
    else {
        recvr->amt_read += ret;
    }
}

static void
read_header(receiver_t * recvr) {
    do_read(recvr);

    NEW_FRAME(FMTS("%p", "%p", "%d", "%d", "%d", "%d"),
              VARS(recvr,
                   recvr->buf,
                   recvr->fd,
                   recvr->amt_read,
                   recvr->amt_expected,
                   recvr->buf_size));

    // fall through if still need to read more of header
    if (recvr->amt_read == recvr->amt_expected) {
        uint32_t total_expec = IO_DATA_LEN(recvr->buf);
        if (total_expec == 0) {
            // throw away this read
            PRINT(LOW_VERBOSE, "Empty read on fd: %d\n", recvr->fd);
            reset_recvr_rd_state(recvr);
        }
        else {
            if (total_expec > recvr->buf_size) {

                // minimum is exponential growth of buf_size so worst case O(N)
                const uint32_t _old_buf_size = recvr->buf_size;
                recvr->buf_size = MAX(total_expec, 2 * recvr->buf_size);
                recvr->buf = (uint8_t *)realloc(recvr->buf, recvr->buf_size);
                if (recvr->buf == NULL) {
                    errdie("Realloc of buf failed\n");
                }
                memset(recvr->buf + _old_buf_size,
                       0,
                       recvr->buf_size - _old_buf_size);

                IO_DATA_LEN(recvr->buf) = total_expec;
            }
            recvr->amt_expected += (total_expec - HEADER_SIZE);
            recvr->rd_state = READING_BODY;
        }
    }
}


io_data *
read_body(receiver_t * recvr) {
    DBG_ASSERT(IO_DATA_LEN(recvr->buf) <= recvr->amt_expected,
               "Error: expected length insufficient for data length\n\t"
               "expec   : %d\n\t"
               "dlength : %d\n",
               recvr->amt_expected,
               IO_DATA_LEN(recvr->buf));

    do_read(recvr);

    if (recvr->amt_read == recvr->amt_expected) {

        /*
        ACQUIRE_OUTBUF((recvr->outbuf));
                if (recvr->amt_read > recvr->outbuf_size) {
            recvr->outbuf_size = MAX(recvr->amt_read, 2 * recvr->outbuf_size);
            myfree(OUTBUF_PTR(recvr->outbuf));
            io_data * _temp_outbuf =
                (io_data *)mycalloc(recvr->outbuf_size, sizeof(uint8_t));

            // this is just so that lock holds
            ACQUIRE_OUTBUF(_temp_outbuf);

            recvr->outbuf = _temp_outbuf;
            }*/

        io_data * _temp_outbuf =
            (io_data *)mycalloc(recvr->amt_read, sizeof(uint8_t));
        memcpy(_temp_outbuf, recvr->buf, recvr->amt_read);
        reset_recvr_rd_state(recvr);
        return _temp_outbuf;
    }
    return NULL;
}


io_data *
handle_read(receiver_t * recvr) {


    PRINT(HIGH_VERBOSE, "Read event triggered for fd [%d]\n", recvr->fd);

    if (recvr->rd_state == READING_NONE) {
        PRINT(HIGH_VERBOSE, "Case: READING_NONE\n");
        DBG_ASSERT(recvr->amt_read == 0 && recvr->amt_expected == 0,
                   "Error: recvr meta data corrupted: %d -> (%d, %d)\n",
                   recvr->rd_state,
                   recvr->amt_read,
                   recvr->amt_expected);

        recvr->amt_expected = HEADER_SIZE;
        recvr->rd_state     = READING_HEADER;
        read_header(recvr);
    }
    if (recvr->rd_state == READING_HEADER) {
        PRINT(HIGH_VERBOSE,
              "Case: READING_HEADER %d/%d\n",
              recvr->amt_read,
              recvr->amt_expected);
        DBG_ASSERT(recvr->amt_read < HEADER_SIZE,
                   "Error: to much data(%d) to be in state READING_HEADER\n",
                   recvr->amt_read);

        read_header(recvr);
    }
    else if (recvr->rd_state == READING_BODY) {
        PRINT(HIGH_VERBOSE,
              "Case: READING_BODY %d/%d\n",
              recvr->amt_read,
              recvr->amt_expected);
        DBG_ASSERT(recvr->amt_read >= HEADER_SIZE,
                   "Error: not enough data(%d) to be in state READING_BODY\n",
                   recvr->amt_read);
        return read_body(recvr);
    }
    else {
        die("Receiver in invalid rd_state: %d\n", recvr->rd_state);
    }
    return NULL;
}

void
handle_write(receiver_t * recvr) {
    DBG_ASSERT(recvr->ev_state == WRITING,
               "Error ev_state != writing (%d != %d)\n",
               recvr->ev_state,
               WRITING);

    // if priority we already have acquired the lock successfully
    ACQUIRE_OUTBUF(recvr->outbuf);


    PRINT(HIGH_VERBOSE,
          "Writing[%p (%d)]\n",
          OUTBUF_PTR(recvr->outbuf),
          recvr->outbuf_cur_size);

    uint32_t ret = myrobustwrite(recvr->fd,
                                 OUTBUF_PTR(recvr->outbuf),
                                 recvr->outbuf_cur_size);

    DBG_ASSERT(ret == recvr->outbuf_cur_size,
               "Error didn't write all data %d/%d\n",
               ret,
               recvr->outbuf_cur_size);

    recvr->outbuf_cur_size = 0;

    // if priority we will leave unlocking up to caller

    RELEASE_OUTBUF(recvr->outbuf);
    reset_recvr_event(recvr, &handle_event, EV_READ, WAITING);
}

void
set_owner(receiver_t * recvr, void * owner) {
    recvr->my_owner = owner;
}

void
set_assos(receiver_t * recvr, void * assos, destruct_handler dest) {
    recvr->my_assos = assos;
    recvr->dest     = dest;
}
