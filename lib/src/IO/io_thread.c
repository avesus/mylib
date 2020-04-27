#include <IO/io_thread.h>

static uint32_t
count_recvrs(io_thread_t * io_thr) {
    uint64_t idx_mask = io_thr->idx_mask;
    uint64_t recvr_mask_idx;

    uint32_t sum = 0;

    uint32_t total_idx = io_thr->max_receivers / sizeof_bits(uint64_t);
    while (idx_mask) {
        total_idx--;
        ff1_asm_tz(idx_mask, recvr_mask_idx);
        idx_mask ^= ((1UL) << recvr_mask_idx);

        sum += bitcount_64(io_thr->recvr_idx_mask[recvr_mask_idx]);
    }
    return sum + (total_idx * sizeof_bits(uint64_t));
}


static uint64_t
find_set_recvr_idx(io_thread_t * io_thr) {
    uint64_t idx_mask = io_thr->idx_mask;
    uint64_t recvr_mask_idx;
    uint64_t recvr_idx;

    DBG_ASSERT(idx_mask,
               "Error no space in io thread %d\n",
               io_thr->my_thread_idx);

    ff1_asm_tz(idx_mask, recvr_mask_idx);

    DBG_ASSERT((~io_thr->recvr_idx_mask[recvr_mask_idx]),
               "Error invalid bit vector indexing in io_thread %d\n",
               io_thr->my_thread_idx);

    ff0_asm_tz(io_thr->recvr_idx_mask[recvr_mask_idx], recvr_idx);
    io_thr->recvr_idx_mask[recvr_mask_idx] |= ((1UL) << recvr_idx);
    if (!(~io_thr->recvr_idx_mask[recvr_mask_idx])) {
        io_thr->idx_mask ^= ((1UL) << recvr_mask_idx);
    }
    PRINT(HIGH_VERBOSE,
          "find_set on thread (%d)\n\t"
          "mask1 : %lx\n\t"
          "idx1  : %lu\n\t"
          "mask2 : %lx\n\t"
          "idx2  : %lu\n",
          io_thr->my_thread_idx,
          idx_mask,
          recvr_mask_idx,
          io_thr->recvr_idx_mask[recvr_mask_idx],
          recvr_idx);


    return recvr_idx + (recvr_mask_idx * sizeof_bits(uint64_t));
}


static void
change_avail_status(io_thread_t * io_thr) {
    const uint64_t _change_bit = ((1UL) << io_thr->my_thread_idx);
    const uint64_t _dbg_res = __atomic_xor_fetch(io_thr->parent_avail_threads,
                                                 _change_bit,
                                                 __ATOMIC_RELAXED);

    DBG_ASSERT((!(_dbg_res & _change_bit)) ==
                   (!(io_thr->max_receivers > (count_recvrs(io_thr) + 1))),
               "Error: acceptor thread mask improperly set\n\t"
               "thread_idx  : %d\n\t"
               "thread_max  : %d\n\t"
               "thread_cur  : %d\n\t"
               "accptr_mask : %lx\n",
               io_thr->my_thread_idx,
               io_thr->max_receivers,
               count_recvrs(io_thr),
               *(io_thr->parent_avail_threads));
}


void
drop_recvr(io_thread_t * io_thr, uint32_t recvr_idx) {
    PRINT(MED_VERBOSE,
          "Freeing receiver %d from thread %d\n",
          recvr_idx,
          io_thr->my_thread_idx);


    const uint32_t _vec_idx = recvr_idx / sizeof_bits(uint64_t);
    const uint32_t _vec_pos = recvr_idx % sizeof_bits(uint64_t);

    pthread_mutex_lock(&(io_thr->m));
    io_thr->recvrs[recvr_idx] = NULL;
    DBG_ASSERT(recvr_idx < io_thr->max_receivers,
               "Error dropping out of bounds receiver\n");

    DBG_ASSERT(io_thr->idx_mask & ((1UL) << _vec_idx) &&
                   io_thr->recvr_idx_mask[_vec_idx] & ((1UL) << _vec_pos),
               "Error dropping unset receiver\n\t",
               "mask1   : %lx\n\t"
               "idx1    : %d\n\t"
               "mask2   : %lx\n\t"
               "idx2    : %d\n",
               io_thr->idx_mask,
               _vec_idx,
               io_thr->recvr_idx_mask[_vec_idx],
               _vec_pos);

    uint32_t unavail = !(~io_thr->recvr_idx_mask[_vec_idx]);
    io_thr->recvr_idx_mask[_vec_idx] ^= ((1UL) << _vec_pos);

    // change availability in parent vector if was previously full
    if (unavail) {
        unavail = !(~io_thr->idx_mask);
        io_thr->idx_mask ^= ((1UL) << _vec_idx);
        if (unavail) {
            change_avail_status(io_thr);
        }
    }


    pthread_mutex_unlock(&(io_thr->m));
}


static void *
handle_command(void * arg, io_data * data_buf) {
    receiver_t * recvr = (receiver_t *)arg;
    if (data_buf->length == (HEADER_SIZE + strlen("quit")) &&
        (!strcmp(data_buf->data, "quit"))) {

        drop_recvr((io_thread_t *)(recvr->my_parent), recvr->my_recvr_idx);
        free_recvr(recvr);
    }
    myfree(data_buf);
    return NULL;
}


receiver_t *
register_new_connection(io_thread_t * io_thr, int32_t new_conn_fd) {
    pthread_mutex_lock(&(io_thr->m));
    const uint32_t _nrecvrs  = count_recvrs(io_thr);
    uint64_t       recvr_idx = find_set_recvr_idx(io_thr);
    pthread_mutex_unlock(&(io_thr->m));

    if ((_nrecvrs + 1) == io_thr->max_receivers) {
        change_avail_status(io_thr);
    }

    DBG_ASSERT(io_thr->max_receivers > _nrecvrs,
               "Error selected thread (%d) is full!\n",
               io_thr->my_thread_idx);


    receiver_t * new_recvr = init_recvr(new_conn_fd,
                                        0,
                                        recvr_idx,
                                        (void *)io_thr,
                                        io_thr->thr_ev_base,
                                        &handle_command);


    DBG_ASSERT(io_thr->recvrs[recvr_idx] == NULL,
               "Error recvr idx (%lu) already taken\n",
               recvr_idx);

    PRINT(MED_VERBOSE,
          "IO_thr[%d] adding new receiver fd[%d] at index[%lx & %lu], "
          "space(%d/%d)\n",
          io_thr->my_thread_idx,
          new_conn_fd,
          recvr_idx,
          io_thr->recvr_idx_mask[recvr_idx / sizeof_bits(uint64_t)],
          count_recvrs(io_thr),
          io_thr->max_receivers);


    io_thr->recvrs[recvr_idx] = new_recvr;
    return new_recvr;
}


io_thread_t *
init_thread(volatile uint64_t * parent_avail_threads,
            uint32_t            thread_idx,
            uint32_t            max_receivers) {

    const uint32_t _max_receivers =
        max_receivers ? ROUNDUP_P2(max_receivers, sizeof_bits(uint64_t))
                      : DEFAULT_MAX_RECEIVERS;
    const uint32_t _extra_alloc = (_max_receivers - 1) / sizeof_bits(uint64_t);

    io_thread_t * new_io_thr = (io_thread_t *)mycalloc(
        1,
        sizeof(io_thread_t) + _extra_alloc * sizeof(uint64_t));

    new_io_thr->thr_ev_base = event_base_new();
    if (new_io_thr->thr_ev_base == NULL) {
        errdie("Couldn't initialize event base\n");
    }

    pthread_mutex_init(&(new_io_thr->m), NULL);

    DBG_ASSERT(thread_idx < sizeof_bits(uint64_t), "Max threads exceeded!\n");

    PRINT(LOW_VERBOSE, "New IO thread at index %d\n", thread_idx);

    new_io_thr->max_receivers = _max_receivers;

    // set invalid slots within array of connection bitvectors
    new_io_thr->idx_mask = (((1UL) << (_extra_alloc + 1)) - 1);

    PRINT(MED_VERBOSE,
          "New IO thread vectors intialized\n\t"
          "max_recvrs   : %d\n\t"
          "extra_alloc  : %d\n\t"
          "idx_mask     : %lx\n\t"
          "recvr_mask[0]: %lx\n\t"
          "recvr_mask[n]: %lx\n",
          _max_receivers,
          _extra_alloc,
          new_io_thr->idx_mask,
          new_io_thr->recvr_idx_mask[0],
          new_io_thr->recvr_idx_mask[_extra_alloc]);

    new_io_thr->my_thread_idx        = thread_idx;
    new_io_thr->parent_avail_threads = parent_avail_threads;

    new_io_thr->recvrs = (receiver_t **)mycalloc(new_io_thr->max_receivers,
                                                 sizeof(receiver_t *));


    NEW_FRAME(FMTS("%p", "%p", "%d", "%d"),
              VARS(new_io_thr,
                   new_io_thr->recvrs,
                   new_io_thr->max_receivers,
                   thread_idx));


    return new_io_thr;
}


static void *
io_thread_loop(void * arg) {
    io_thread_t * io_thr = (io_thread_t *)arg;
    pthread_detach(pthread_self());
    if (event_base_loop(io_thr->thr_ev_base, EVLOOP_NO_EXIT_ON_EMPTY) != 0) {
        errdie("Error running thread io event loop: %p\n", io_thr->thr_ev_base);
    }
    return NULL;
}

void
start_thread(io_thread_t * io_thr) {
    mypthread_create(&(io_thr->tid), NULL, io_thread_loop, io_thr);
}


void
free_thread(io_thread_t * io_thr) {
    pthread_mutex_lock(&(io_thr->m));
    PRINT(MED_VERBOSE, "Exitting thread: %d\n", io_thr->my_thread_idx);
    if (event_base_loopbreak(io_thr->thr_ev_base) == (-1)) {
        errdie("Couldn't cancel thread event loop. This is unrecoverable\n");
    }

    uint64_t idx_mask = io_thr->idx_mask, recvr_idx_mask;
    uint64_t recvr_mask_idx, recvr_idx;
    while (idx_mask) {
        ff1_asm_tz(idx_mask, recvr_mask_idx);
        idx_mask ^= ((1UL) << recvr_mask_idx);
        recvr_idx_mask = io_thr->recvr_idx_mask[recvr_mask_idx];

        while (recvr_idx_mask) {
            ff1_asm_tz(recvr_idx_mask, recvr_idx);
            recvr_idx_mask ^= ((1UL) << recvr_idx);

            free_recvr(io_thr->recvrs[recvr_idx +
                                      recvr_mask_idx * sizeof_bits(uint64_t)]);
        }
    }
    // intentionally not freeing here. The program is closing so this is not a
    // meaningful memory leak and can potentially cause a seg fault if another
    // call is hanging at the mutex
}
