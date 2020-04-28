#ifndef _SYNC_QUE_H_
#define _SYNC_QUE_H_

#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>

#include <helpers/opt.h>

#include <broadway-config.h>
#include <broadway/play.h>

#include <IO/receiver.h>



using namespace std;

// the type that lets a worker who pops item get enough info to read
// the file/start reciting
struct p_info {
    string           name;
    size_t           frag_num;
    string           file;
    shared_ptr<Play> active_play;
    string *         agr_outbuf;
    receiver_t *     recvr;
    int32_t * volatile frags_left;
    int32_t * volatile progress_state;
    p_info() {}
};


class sync_que {

    mutex m;

    queue<p_info> que;

   public:
    condition_variable cv;
    volatile uint64_t  done = 0;
    void               push(p_info p);
    void               clear();
    p_info             pop();

    size_t
    size() {
        return que.size();
    }
};


#endif
