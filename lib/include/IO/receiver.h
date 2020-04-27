#ifndef _RECEIVER_H_
#define _RECEIVER_H_

#include <IO/io_header.h>
#include <event.h>
#include <helpers/bits.h>
#include <helpers/debug.h>
#include <helpers/locks.h>
#include <helpers/util.h>


#define ACQUIRE_OUTBUF(X) (lb_writeLock((uint64_t * volatile)(&(X))))
#define RELEASE_OUTBUF(X) (lb_unlock_wr((uint64_t * volatile)(&(X))))

#define OUTBUF_PTR(X) ((uint8_t *)low_bits_get_ptr((X)))

#define PRIORITY_OUTBUF_MASK (lb_write_locked - 1)
// this is kind of hacky but basically will atomically decrement ptr
#define SET_PRIORITY(X) (lb_unlock_rd((uint64_t * volatile)(&(X))))
#define IS_PRIORITY(X)  (low_bits_get((X)) == PRIORITY_OUTBUF_MASK)


#define ACQUIRE 0x1
#define RELEASE 0x2

#define INCOMPLETE 0
#define COMPLETE   1

enum event_states {
    WAITING  = 0,
    READING  = 1,
    HANDLING = 2,
    WRITING  = 3,

};

enum reader_states {
    READING_NONE   = 0,
    READING_HEADER = 1,
    READING_BODY   = 2,
};

#define MIN_BUF_SIZE sizeof(io_data_t)


typedef void * (*cmd_handler)(void *, io_data *);
typedef void (*destruct_handler)(void *);

typedef struct receiver {
    int32_t  fd;
    uint32_t my_recvr_idx;

    uint32_t amt_read;
    uint32_t amt_expected;

    uint8_t * buf;
    // possibly optimize this so that size it taken from malloc header
    uint32_t buf_size;

    volatile uint8_t * outbuf;
    uint32_t           outbuf_cur_size;
    uint32_t           outbuf_size;

    reader_states rd_state;
    event_states  ev_state;

    void * my_parent;
    void * my_assos;
    void * my_owner;

    struct event        recvr_ev;
    struct event_base * my_base;

    cmd_handler      rd_handle;
    destruct_handler dest;
} receiver_t;


void clear_recvr_outbuf(receiver_t * recvr);
void store_recvr_outbuf(receiver_t * recvr,
                        uint8_t *    to_add,
                        uint32_t     size_to_add,
                        uint32_t     lock_flags);
void prepare_send_recvr(receiver_t * recvr, HEADER_TYPE hdr, uint8_t * data);

void free_recvr(receiver_t * recvr);


receiver_t * init_stdin_recvr(uint32_t            init_buf_size,
                              void *              parent,
                              struct event_base * base,
                              cmd_handler         rd_handle);


receiver_t * init_recvr(int32_t             fd,
                        uint32_t            init_buf_size,
                        uint32_t            recvr_idx,
                        void *              parent,
                        struct event_base * base,
                        cmd_handler         rd_handle);

io_data * handle_read(receiver_t * recvr);
void      handle_write(receiver_t * recvr);

void reset_recvr_event(receiver_t * recvr,
                       void         ev_handler(const int, const short, void *),
                       int32_t      new_flags,
                       event_states ev_state);

void handle_stdin_event(const int fd, const short which, void * arg);
void handle_event(const int fd, const short which, void * arg);


void set_assos(receiver_t * recvr, void * assos, destruct_handler dest);
void set_owner(receiver_t * recvr, void * owner);
#endif
