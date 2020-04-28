#include <IO/acceptor.h>

static void
default_sigint_handler(const int32_t sig) {
    DBG_ASSERT(
        sig == SIGINT,
        "Error sigint somehow called without SIGINT...\nClearly I messed up\n");

    fprintf(stderr, "SIGINT handler evoked\n");
    exit(0);
}

static int32_t
prep_accptr_socket(char * ip_addr, uint32_t portno) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(portno);
    inet_aton(ip_addr, &addr.sin_addr);

    int32_t socket_flags = 1;
    int32_t socket_fd    = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd <= 0) {
        errdie("Failure creating socket\n");
    }

    setsockopt(socket_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               (void *)&socket_flags,
               sizeof(socket_flags));
    setsockopt(socket_fd,
               SOL_SOCKET,
               SO_KEEPALIVE,
               (void *)&socket_flags,
               sizeof(socket_flags));


    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) ==
        (-1)) {
        errdie("Failure binding socket\n");
    }

    if (listen(socket_fd, LISTEN_QUE) == (-1)) {
        errdie("Failure listening on socket\n");
    }

    return socket_fd;
}

static io_thread_t *
add_new_io_thread(acceptor_t * accptr) {
    const uint32_t _new_thread_idx =
        __atomic_fetch_add(&(accptr->nthreads), 1, __ATOMIC_RELAXED);

    DBG_ASSERT(_new_thread_idx < sizeof_bits(uint64_t),
               "Error cant add new thread: OOB\n");

    const uint64_t _change_bit = ((1UL) << _new_thread_idx);
    const uint64_t _dbg_res    = __atomic_xor_fetch(&(accptr->avail_threads),
                                                 _change_bit,
                                                 __ATOMIC_RELAXED);
    DBG_ASSERT(
        _dbg_res & _change_bit,
        "Error: something went seriously wrong with acceptor thread mask\n\t"
        "new_idx : %d\n\t"
        "mask    : %lx\n",
        _new_thread_idx,
        _dbg_res);


    accptr->io_threads[_new_thread_idx] =
        init_thread(&(accptr->avail_threads), _new_thread_idx, 0);
    start_thread(accptr->io_threads[_new_thread_idx]);
    return accptr->io_threads[_new_thread_idx];
}

static void
add_connection(acceptor_t * accptr, int32_t new_conn_fd) {
    io_thread_t * io_thr     = NULL;
    uint64_t      thread_idx = 0;
    if (accptr->avail_threads == 0) {
        pthread_mutex_lock(&(accptr->m));
        if (accptr->avail_threads == 0) {
            PRINT(MED_VERBOSE, "New connection[%d] case 1\n", new_conn_fd);
            io_thr     = add_new_io_thread(accptr);
            thread_idx = io_thr->my_thread_idx;
        }
        pthread_mutex_unlock(&(accptr->m));
    }
    else {
        PRINT(MED_VERBOSE, "New connection[%d] case 2\n", new_conn_fd);

        // to try and spread connections among threads
        const uint64_t _last_tids_mask = ~(((1UL) << accptr->last_tid) - 1);
        const uint64_t _avail_thread_max =
            (accptr->avail_threads & _last_tids_mask)
                ? (accptr->avail_threads & _last_tids_mask)
                : accptr->avail_threads;

        ff1_asm_tz(_avail_thread_max, thread_idx);
        DBG_ASSERT(thread_idx < accptr->nthreads,
                   "Error: out of bounds thread_idx\n\t"
                   "avail_mask  : %lx\n\t"
                   "thread_idx  : %lu\n\t"
                   "nthreads    : %d\n",
                   _avail_thread_max,
                   thread_idx,
                   accptr->nthreads);

        io_thr = accptr->io_threads[thread_idx];
    }
    accptr->last_tid = thread_idx + 1;

    PRINT(MED_VERBOSE,
          "New connection [%d] to thread [%lu]\n",
          new_conn_fd,
          thread_idx);

    DBG_ASSERT(io_thr != NULL,
               "Error NULL io_thread for idx (%lu)\n",
               thread_idx);

    DBG_ASSERT(io_thr->my_thread_idx == thread_idx,
               "Error: thread idxs' dont match: %d != %lu\n",
               io_thr->my_thread_idx,
               thread_idx);

    receiver_t * new_recvr = register_new_connection(io_thr, new_conn_fd);
    if (accptr->my_owner && accptr->init) {
        accptr->init(accptr->my_owner, new_recvr);
    }
}

void
do_accept(const int fd, const short which, void * arg) {
    acceptor_t * accptr = (acceptor_t *)arg;

    socklen_t               addrlen;
    struct sockaddr_storage addr;

    int32_t new_conn_fd =
        accept(accptr->accept_fd, (struct sockaddr *)&addr, &addrlen);
    if (new_conn_fd == (-1)) {
        errdie("Failed to accept new connection\n");
    }
    PRINT(LOW_VERBOSE, "Accepted new connection with fd: %d\n", new_conn_fd);

    add_connection(accptr, new_conn_fd);
}

void
free_accptr(acceptor_t * accptr) {
    pthread_mutex_lock(&(accptr->m));

    if (event_del(&(accptr->accptr_ev)) == -1) {
        errdie("Couldn't delete acceptor event\n");
    }

    PRINT(MED_VERBOSE, "Exitting acceptor\n");
    if (event_base_loopbreak(accptr->accptr_base) == (-1)) {
        errdie("Couldn't cancel acceptor event loop. This is unrecoverable\n");
    }

    close(accptr->accept_fd);

    uint64_t avail_mask = accptr->avail_threads;
    uint64_t avail_idx;

    // free all active threads
    while (avail_mask) {
        ff1_asm_tz(avail_mask, avail_idx);
        avail_mask ^= ((1UL) << avail_idx);
        free_thread(accptr->io_threads[avail_idx]);
    }
    // intentionally not freeing here. The program is closing so this is not a
    // meaningful memory leak and can potentially cause a seg fault if another
    // call is hanging at the mutex
}


acceptor_t *
init_acceptor(uint32_t    init_nthreads,
              char *      ip_addr,
              uint32_t    portno,
              void *      owner,
              sig_handler custum_sigint_handler,
              owner_init  init) {


    acceptor_t * new_accptr = (acceptor_t *)mycalloc(1, sizeof(acceptor_t));

    new_accptr->accptr_base = event_base_new();
    if (new_accptr->accptr_base == NULL) {
        errdie("Couldn't initialize acceptor event base\n");
    }


    new_accptr->my_owner = owner;
    new_accptr->init     = init;

    pthread_mutex_init(&(new_accptr->m), NULL);

    const uint32_t _nthreads = init_nthreads ? init_nthreads : DEFAULT_NTHREADS;

    new_accptr->avail_threads = ((1UL) << _nthreads) - 1;
    new_accptr->nthreads      = _nthreads;
    new_accptr->io_threads =
        (io_thread_t **)mycalloc(sizeof_bits(uint64_t), sizeof(io_thread_t *));

    for (uint32_t i = 0; i < _nthreads; i++) {
        new_accptr->io_threads[i] =
            init_thread(&(new_accptr->avail_threads), i, 0);
    }

    int32_t accept_fd = prep_accptr_socket(ip_addr, portno);

    make_nonblock(accept_fd);
    new_accptr->accept_fd = accept_fd;

    // this is off. Basically this is just a lightweight way for me to grab
    // symbols when trying to debu a data race that GDB's slowdown will hide.
    NEW_FRAME(FMTS("%p", "%d", "%d"),
              VARS(new_accptr, new_accptr->nthreads, new_accptr->accept_fd));

    if (owner != NULL && custum_sigint_handler != NULL) {

        event_set(&(new_accptr->sigint_ev),
                  SIGINT,
                  EV_SIGNAL,
                  custum_sigint_handler,
                  (void *)owner);

        event_base_set(new_accptr->accptr_base, &(new_accptr->sigint_ev));
        if (event_add(&(new_accptr->sigint_ev), NULL) == (-1)) {
            errdie("Unable to add acceptor event\n");
        }
    }
    else {
        signal(SIGINT, default_sigint_handler);
    }

    event_set(&(new_accptr->accptr_ev),
              accept_fd,
              EV_READ | EV_PERSIST,
              do_accept,
              (void *)new_accptr);

    event_base_set(new_accptr->accptr_base, &(new_accptr->accptr_ev));

    if (event_add(&(new_accptr->accptr_ev), NULL) == (-1)) {
        errdie("Unable to add acceptor event\n");
    }
    for (uint32_t i = 0; i < _nthreads; i++) {
        start_thread(new_accptr->io_threads[i]);
    }
    return new_accptr;
}

void
start_accepting(acceptor_t * accptr) {
    if (event_base_loop(accptr->accptr_base, 0) != 0) {
        errdie("Error running acceptor event loop\n");
    }
}
