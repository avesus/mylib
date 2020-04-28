#ifndef _DIRECTOR_H_
#define _DIRECTOR_H_

#include <condition_variable>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <numeric>
#include <queue>
#include <sstream>
#include <string>

#include <helpers/debug.h>
#include <helpers/util.h>

#include <broadway-config.h>
#include <broadway/player.h>
#include <broadway/sync_que.h>

#include <IO-config.h>
#include <IO/connector.h>
#include <IO/io_header.h>


using namespace std;

#define STATUS_LEN 12
#define INDEX_LEN  4


struct play_config_info {
    vector<string>   names;      // names of all roles
    vector<ifstream> configs;    // will store where each scenes config file is
    vector<int32_t>  n_players;  // number of players for each scene
    play_config_info() {}
};

/* Director class (really director & producer). Will open play files
   and based on the script get necessary players (hence producer) to
   recite their lines via the Play class */
class Director {
    connector_t *    connect;   // for connecting to producer and handling IO
    vector<ifstream> files;     // ifstream with script file(s)
    vector<string>   titles;    // titles of play this director has
    vector<string>   statuses;  // status of each play
    vector<uint32_t> nparts;    // total number of parts for play.
    vector<play_config_info>   pconf;    // config info for each play
    vector<shared_ptr<Play>>   p;        // play class to actually do the scenes
    vector<shared_ptr<Player>> players;  // all necessary players
    mutex                      m;
    condition_variable         cv;
    mutex                      m_cue;
    condition_variable         cv_cue;
    thread                     t_cue;


    sync_que frag_que;  // que for doing leader follower among scenes

   public:
    volatile int32_t shutdown;
    volatile int32_t in_progress;
    int32_t          last_recognized;
    int32_t          last_play_idx;

    Director(const string & name, char * ip_addr, uint32_t portno);
    Director(const string & name,
             uint32_t       min_players,
             char *         ip_addr,
             uint32_t       portno);

    ~Director();
    // coordinates the recitation of each play fragment

    int32_t find_req_play(string req_play_name);
    void    cue_wrapper();
    void    cue(uint32_t idx);
    string  format_avails();
    int32_t update_status_idx(uint32_t idx);
    void    send_avails();
    void    send_content();
    bool    cue_exit();
    void    cue_enter();
    void    cue_idx(uint32_t idx);
    void    start_directing();
};


#endif
