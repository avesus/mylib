#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <set>
#include <string>
#include <thread>

#include <broadway-config.h>
#include <broadway/line.h>
#include <broadway/play.h>
#include <broadway/sync_que.h>

#include <IO-config.h>
#include <IO/receiver.h>

#define READY     0
#define CANCELLED 1
#define ONGOING 2


using namespace std;

class Play;


/* Player class that actually stores the lines for each character and
   is coordinated by play class */
class Player {
    string    name;
    set<line> lines;
    Play &    p;
    ifstream  ifs;
    thread    t;

   public:
    Player(Play & p) : p(p) {}

    void read(string name, string file);

    void act(p_info pi);

    void work(sync_que & q, condition_variable & cv_dir);

    void enter(sync_que & q, condition_variable & cv_dir);

    bool exit();
};

#endif
