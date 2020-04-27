#include <broadway/sync_que.h>

// Basic que that allows safe multithreaded push/pops. This is how
// config files for characters are given to players by the director
void
sync_que::push(p_info p) {

    std::lock_guard<mutex> lock(this->m);
    this->que.push(p);
    cv.notify_all();
}


p_info 
sync_que::pop() {
    std::unique_lock<std::mutex> lock(this->m);
    cv.wait(lock, [&] { return (this->que.size() || this->done); });
    if (this->done && this->que.size() == 0) {
        string empty_s = "";
        p_info empty(empty_s);
        return empty;
    }
    p_info ret = que.front();
    que.pop();
    return ret;
}
