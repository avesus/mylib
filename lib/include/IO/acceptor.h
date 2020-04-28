#ifndef _ACCEPTOR_H_
#define _ACCEPTOR_H_

#include <IO-config.h>
#include <IO/io_thread.h>
#include <IO/receiver.h>

typedef void (*sig_handler)(const int, const short, void *);
typedef void (*owner_init)(void *, receiver_t *);

typedef struct acceptor {
    uint32_t nthreads;
    uint32_t last_tid;

    pthread_t *       tids;
    io_thread_t **    io_threads;
    volatile uint64_t avail_threads;
    pthread_mutex_t   m;

    struct event        sigint_ev;
    struct event        accptr_ev;
    struct event_base * accptr_base;

    void *     my_owner;
    owner_init init;

    int32_t accept_fd;

} acceptor_t;

void free_accptr(acceptor_t * accptr);

acceptor_t * init_acceptor(uint32_t   init_nthreads,
                           char *     ip_addr,
                           uint32_t   portno,
                           void *     owner,
                           sig_handler custum_sigint_handler,
                           owner_init init);

void start_accepting(acceptor_t * accptr);
#endif
