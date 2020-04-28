#include <broadway/producer.h>

#define ACQUIRE_USR_DATA(X) (lb_writeLock((uint64_t * volatile)(&(X))))
#define RELEASE_USR_DATA(X) (lb_unlock_wr((uint64_t * volatile)(&(X))))

#define USR_DATA_PTR(X) ((io_data *)low_bits_get_ptr((X)))

static void
my_sigint_handler(const int fd, const short which, void * arg) {
    PRINT(LOW_VERBOSE, "Handling SIGINT\n");
    producer *   this_producer = (producer *)arg;
    this_producer->~producer();
}

static void
send_data(receiver_t * recvr, TYPE_TYPE type, HEADER_TYPE hdr, char * data) {

    // assume header == 0 means needs to set header
    if (hdr == 0) {
        hdr = HEADER_SIZE + TYPE_SIZE + strlen(data);
    }

    DBG_ASSERT(IS_VALID_TYPE(type), "Error invalid type: %d\n", type);

    PRINT(LOW_VERBOSE, "writing: %s\n", data);
    store_recvr_outbuf(recvr, (uint8_t *)(&hdr), HEADER_SIZE, ACQUIRE);
    store_recvr_outbuf(recvr, (uint8_t *)(&type), TYPE_SIZE, 0);
    store_recvr_outbuf(recvr,
                       (uint8_t *)data,
                       (hdr - (HEADER_SIZE + TYPE_SIZE)),
                       RELEASE);

    reset_recvr_event(recvr, &handle_event, EV_WRITE, WRITING);
}


static void
send_type(receiver_t * recvr, TYPE_TYPE type) {
    HEADER_TYPE hdr = HEADER_SIZE + TYPE_SIZE;

    PRINT(HIGH_VERBOSE, "Sending[%d] new type [%d]\n", hdr, type);

    store_recvr_outbuf(recvr, (uint8_t *)(&hdr), HEADER_SIZE, ACQUIRE);
    store_recvr_outbuf(recvr, (uint8_t *)(&type), TYPE_SIZE, RELEASE);

    reset_recvr_event(recvr, &handle_event, EV_WRITE, WRITING);
}

static void *
handle_net_cmd(void * arg, io_data * data_buf) {
    receiver_t * recvr = (receiver_t *)arg;

    DBG_ASSERT(recvr->fd >= 0, "Error invalid fd in cmd handler\n");

    TYPE_TYPE type = data_buf->data[TYPE_IDX];
    DBG_ASSERT(IS_VALID_TYPE(type), "Error invalid type: %d\n", type);

    if (type == KILL_MSG) {
        PRINT(HIGH_VERBOSE, "Killing reciver: %d\n", recvr->my_recvr_idx);
        drop_recvr((io_thread_t *)(recvr->my_parent), recvr->my_recvr_idx);
        free_recvr(recvr);
        myfree(data_buf);
        // kill self protocol
    }
    else if (type == AVAIL_MSG) {
        producer *   this_producer = (producer *)recvr->my_owner;
        arr_node_t * dir_node      = (arr_node_t *)recvr->my_assos;
        DBG_ASSERT(this_producer != NULL && dir_node != NULL,
                   "Unset recvr got event\n");
        ACQUIRE_USR_DATA(dir_node->usr_data);
        myfree(USR_DATA_PTR(dir_node->usr_data));
        // setting here will unlock
        dir_node->usr_data = (volatile void *)data_buf;
        this_producer->print_director_data();
    }
    else if (type == CONTENT_MSG) {
        fprintf(stderr,
                "Received Play Content!\n\"%s\"\n",
                (char *)(&(data_buf->data[DATA_START_IDX])));
        // request to state a given play
        send_type(recvr, AVAIL_MSG);
        myfree(data_buf);
    }
    else if (type == CONTROL_MSG) {

        // director is requesting a ping
        fprintf(stderr,
                "Director Send Non-Content Response: \n\"%s\"\n",
                (char *)(&(data_buf->data[DATA_START_IDX])));
        myfree(data_buf);
    }
    else {
        die("This is impossible...\n");
    }

    return NULL;
}

#define RETURN_CMD                                                             \
    stdin_recvr->amt_read = 0;                                                 \
    stdin_recvr->rd_state = READING_NONE;                                      \
    return NULL;

static void *
handle_stdin_cmd(void * arg, io_data * data_buf) {
    receiver_t * stdin_recvr   = (receiver_t *)arg;
    producer *   this_producer = (producer *)stdin_recvr->my_owner;

    char * str_buf = (char *)stdin_recvr->buf;
    // drop the newline
    str_buf[stdin_recvr->amt_read - 1] = 0;

    if (!strcmp(str_buf, "exit")) {
        this_producer->~producer();
        RETURN_CMD;
    }
    else if (!strncmp(str_buf, "quit", strlen("quit"))) {
        char *   end     = str_buf + strlen("quit") + 1;
        uint32_t dir_idx = strtol(str_buf + strlen("quit") + 1, &end, 10);
        if (end == str_buf + strlen("quit") + 1) {
            fprintf(stderr, "Invalid command \"%s\"\n", str_buf);
            RETURN_CMD;
        }
        else {
            arr_node_t * dir_node =
                get_node_idx(this_producer->director_list, dir_idx);
            if (dir_node == NULL) {
                fprintf(stderr, "Director index %d does not exist!\n", dir_idx);
                RETURN_CMD;
            }
            receiver_t * recvr = (receiver_t *)(dir_node->usr_key);

            // director response handler will actually clean up the
            // director/recvr
            send_type(recvr, KILL_MSG);
        }
    }
    else if (!strncmp(str_buf, "play", strlen("play"))) {
        uint32_t dir_idx;
        char     play_name[MED_BUF_LEN];
        if (sscanf(str_buf + strlen("play") + 1,
                   "%d %s\n",
                   &dir_idx,
                   play_name) != 2) {
            fprintf(stderr, "Invalid command \"%s\"\n", str_buf);
            RETURN_CMD;
        }
        arr_node_t * dir_node =
            get_node_idx(this_producer->director_list, dir_idx);

        if (dir_node == NULL) {
            fprintf(stderr, "Director index %d does not exist!\n", dir_idx);
            RETURN_CMD;
        }

        receiver_t * recvr = (receiver_t *)(dir_node->usr_key);

        send_data(recvr,
                  CONTENT_MSG,
                  strlen(play_name) + HEADER_SIZE + TYPE_SIZE,
                  play_name);
    }
    else if (!strncmp(str_buf, "show", strlen("show"))) {
        char *   end     = str_buf + strlen("show") + 1;
        uint32_t dir_idx = strtol(str_buf + strlen("show") + 1, &end, 10);
        if (end == str_buf + strlen("show") + 1) {
            fprintf(stderr, "Invalid command \"%s\"\n", str_buf);
            RETURN_CMD;
        }

        arr_node_t * dir_node =
            get_node_idx(this_producer->director_list, dir_idx);
        if (dir_node == NULL) {
            fprintf(stderr, "Director index %d does not exist!\n", dir_idx);
            RETURN_CMD;
        }
        receiver_t * recvr = (receiver_t *)(dir_node->usr_key);

        send_type(recvr, AVAIL_MSG);
    }
    else if (!strncmp(str_buf, "cancel", strlen("cancel"))) {
        char *   end     = str_buf + strlen("cancel") + 1;
        uint32_t dir_idx = strtol(str_buf + strlen("cancel") + 1, &end, 10);
        if (end == str_buf + strlen("cancel") + 1) {
            fprintf(stderr, "Invalid command \"%s\"\n", str_buf);
            RETURN_CMD;
        }

        arr_node_t * dir_node =
            get_node_idx(this_producer->director_list, dir_idx);

        if (dir_node == NULL) {
            fprintf(stderr, "Director index %d does not exist!\n", dir_idx);
            RETURN_CMD;
        }
        receiver_t * recvr = (receiver_t *)(dir_node->usr_key);
        send_type(recvr, CONTROL_MSG);
    }
    else {
        fprintf(stderr, "Invalid command \"%s\"\n", str_buf);
    }


    RETURN_CMD;
}

static void
handle_dir_quit(void * arg) {
    receiver_t * recvr         = (receiver_t *)arg;
    arr_node_t * dir_node      = (arr_node_t *)recvr->my_assos;
    producer *   this_producer = (producer *)recvr->my_owner;

    PRINT(HIGH_VERBOSE, "Start killing recvr: %d\n", recvr->my_recvr_idx);
    this_producer->decr_active_dir();
    remove_node(this_producer->director_list, dir_node);
    PRINT(HIGH_VERBOSE, "Done killing recvr: %d\n", recvr->my_recvr_idx);
}

static void
link_new_recvr(void * me, receiver_t * recvr) {
    PRINT(HIGH_VERBOSE, "Start linking new recvr: %d\n", recvr->my_recvr_idx);
    producer *   this_producer = (producer *)me;
    arr_node_t * director_node =
        add_node(this_producer->director_list, (void *)recvr, NULL);

    // now receiver can update direct data
    set_assos(recvr, (void *)director_node, &handle_dir_quit);
    set_owner(recvr, me);
    this_producer->incr_active_dir();
    recvr->rd_handle = handle_net_cmd;

    // ping director for it to show available plays
    send_type(recvr, AVAIL_MSG);
    PRINT(HIGH_VERBOSE, "Done linking new recvr: %d\n", recvr->my_recvr_idx);
}


producer::producer(uint32_t min_threads, char * _ip_addr, int32_t _portno) {

    if (evthread_use_pthreads() == (-1)) {
        errdie("Couldn't initialize event thread type\n");
    }
    this->ip_addr          = _ip_addr;
    this->portno           = _portno;
    this->active_directors = 0;

    this->director_list = init_alist(0);

    this->accptr = init_acceptor(min_threads,
                                 ip_addr,
                                 portno,
                                 (void *)this,
                                 &my_sigint_handler,
                                 &link_new_recvr);

    this->stdin_recvr = init_stdin_recvr(0,
                                         (void *)this,
                                         this->accptr->accptr_base,
                                         &handle_stdin_cmd);
    set_owner(this->stdin_recvr, (void *)this);
}

producer::~producer() {
    this->kill_all_directors();
    PRINT(LOW_VERBOSE, "Exiting producer\n");
    free_recvr(this->stdin_recvr);

    // free accptr here will be whats responsible for killing all directory
    // connections. Kind of needs to be accptr->io_thread->recvr to avoid
    // concurrency bugs (i.e all events need to be shut down before the
    // recvr can be freed from producer end as nothing is inherently
    // stopping the director from sending back data at same time)
    free_accptr(this->accptr);
    free_alist(this->director_list);
}


void
producer::produce() {
    PRINT(LOW_VERBOSE, "Starting acceptor event loop\n");
    start_accepting(this->accptr);

    // to keep program from exiting until all directors have
}


static void
dir_print_func(arr_node_t * dir) {

    ACQUIRE_USR_DATA(dir->usr_data);
    io_data * formatted_usr_data = USR_DATA_PTR(dir->usr_data);
    if (formatted_usr_data) {
        fprintf(stderr, "%s", &(formatted_usr_data->data[DATA_START_IDX]));
    }
    else {
        fprintf(stderr, "No Plays Available\n");
    }
    RELEASE_USR_DATA(dir->usr_data);
}

void
producer::kill_all_directors() {
    PRINT(MED_VERBOSE,
          "Closing producer\n\t"
          "Active Dir   : %d\n\t"
          "Linked Ptr   : %p\n",
          this->active_directors,
          this->director_list->ll);

    arr_node_t * temp = NULL;
    while (1) {
        temp = __atomic_load_n(&(this->director_list->ll), __ATOMIC_RELAXED);
        if (temp == NULL) {
            break;
        }
        // the node will be removed in director response
        receiver_t * recvr = (receiver_t *)temp->usr_key;
        send_type(recvr, KILL_MSG);

        // ack from the director will cause node removal so temp wont == l
        while (__atomic_load_n(&(this->director_list->ll), __ATOMIC_RELAXED) ==
               temp) {
            // short sleep time so responsive but not too cpu intensive
            do_sleep;
        }
    }
    // kill all directors then wait...
    uint32_t iter = 0;
    while (this->active_directors && iter != MAX_ITER_FORCE_QUIT) {
        // micro sleep to make this a bit less expensive
        do_sleep;

        // eventually we want to exit ungracefully...
        iter++;
    }
    if (iter == MAX_ITER_FORCE_QUIT) {
        fprintf(stderr, "Could not kill all directors. Exiting anyways...\n");
        exit(-1);
    }
}

void
producer::print_director_data() {
    print_nodes(this->director_list, &dir_print_func);
}

void
producer::incr_active_dir() {
    __atomic_add_fetch(&this->active_directors, 1, __ATOMIC_RELAXED);
}

void
producer::decr_active_dir() {
    __atomic_sub_fetch(&this->active_directors, 1, __ATOMIC_RELAXED);
}
