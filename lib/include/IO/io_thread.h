#ifndef _IO_THREAD_H_
#define _IO_THREAD_H_

#include <IO-config.h>
#include <IO/receiver.h>

#include <helpers/opt.h>
#include <helpers/util.h>

#include <event.h>
#include <event2/event.h>
#include <event2/event_compat.h>

#define CONTINUE 0
#define QUIT     1

typedef struct io_thread {
    uint32_t my_thread_idx;

    uint32_t            max_receivers;
    volatile uint64_t * parent_avail_threads;

    struct event_base * thr_ev_base;

    receiver_t ** recvrs;

    pthread_t       tid;
    pthread_mutex_t m;

    uint64_t idx_mask;
    uint64_t recvr_idx_mask[1];


} io_thread_t;

io_thread_t * init_thread(volatile uint64_t * parent_avail_threads,
                          uint32_t            thread_idx,
                          uint32_t            max_receivers);

receiver_t * register_new_connection(io_thread_t * io_thr, int32_t new_conn_fd);

void start_thread(io_thread_t * io_thr);

void drop_recvr(io_thread_t * io_thr, uint32_t recvr_idx);

void free_thread(io_thread_t * io_thr);
#endif
