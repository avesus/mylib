#ifndef _PRODUCER_H_
#define _PRODUCER_H_

#include <broadway-config.h>

#include <IO-config.h>
#include <IO/acceptor.h>
#include <IO/io_thread.h>
#include <IO/receiver.h>

#include <datastructs/arr_list.h>

#include <helpers/util.h>
#include <helpers/locks.h>

#define MAX_ITER_FORCE_QUIT (~(0u))

class producer {
    char *       ip_addr;
    uint32_t     portno;
    volatile uint32_t active_directors;
    receiver_t * stdin_recvr;
    acceptor_t * accptr;
    

   public:
    arr_list_t * director_list;
    
    producer(uint32_t min_threads, char * _ip_addr, int32_t _portno);
    ~producer();
    
    void produce();
    void print_director_data();
    void incr_active_dir();
    void kill_all_directors();
    void decr_active_dir();
};

#endif
