#ifndef _PLAY_H_
#define _PLAY_H_

#include <algorithm>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <helpers/util.h>

#include <broadway-config.h>
#include <broadway/line.h>

using namespace std;

/* Return cases */
enum {
    success,
    cant_open_file,
    join_exception,
    not_ready_to_enter,
    exit_failure
};


class Play {
    vector<string> names;
    size_t         names_idx;
    uint32_t       line_counter;            // current line in recitation
    int32_t        scene_fragment_counter;  // current fragment
    int32_t        on_stage;  // number of character on scene for cur fragment
    int32_t        n_passed;  // number of players unable to do next line
    string         cur_char;  // current character name

    mutex              m;
    condition_variable cv;

   public:
    Play(const vector<string> & inames)
        : names_idx(0),
          line_counter(1),
          scene_fragment_counter(0),
          on_stage(0),
          n_passed(0) {
        for (size_t i = 0; i < inames.size(); i++) {
            this->names.push_back(inames[i]);
        }
        assert(this->names.size());
    }


    // recites a play by coordinating playesr
    void recite(set<line>::iterator & it,
                int32_t               expec_fragment_lines,
                int32_t * volatile progress_state,
                string * agr_outbuf);


    // reset skipped line tracker
    void
    set_on_stage(int32_t n) {
        on_stage = n;
        n_passed = 0;
    }

    // between scenes reset/printout scene name if possible
    void
    reset_counter(string * agr_outbuf) {
        if (this->names[names_idx] != "") {
            (*agr_outbuf) += "\n\n";
            (*agr_outbuf) += this->names[names_idx];
        }

        this->line_counter = 1;
        this->cur_char     = "";
    }


    int32_t
    get_on_stage() {
        return on_stage;
    }
    void
    reset_play_state() {
        this->names_idx              = 0;
        this->line_counter           = 1;
        this->scene_fragment_counter = 0;
        this->on_stage               = 0;
        this->n_passed               = 0;
    }
    // check if ready to start a given scene
    int
    enter(int32_t frag) {
        unique_lock<mutex> l(m);
        if (frag < scene_fragment_counter) {
            return not_ready_to_enter;
        }
        while (frag > scene_fragment_counter) {
            cv.wait(l);
        }
        return success;
    }

    // exits a player. This is pretty complicated because need to keep
    // scene counter coordinated (i.e n_passed & on_stage, as well as
    // fragment counter).
    string
    player_exit(int32_t cancelled) {
        lock_guard<mutex> l(m);

        if (cancelled) {
            this->reset_play_state();
            return "";
        }

        string ret_buf = "";

        // last line is bad
        if (n_passed == --on_stage && on_stage > 0) {
            // nobody could read this line, need to skip it
            ret_buf += "\n****** line ";
            ret_buf += line_counter;
            ret_buf += " skipped ******";

            n_passed = 0;
            cv.notify_all();
        }

        // something went wrong but it shouldnt mess up the entire process
        if (on_stage < 0) {
            // ahh!
            on_stage++;
            return ret_buf;
        }

        // time to go to next fragment (we add some new lines for
        // output cleanliness)
        else if (on_stage == 0) {
            scene_fragment_counter++;
            if (this->names_idx != names.size()) {
                ret_buf += "\n\n\n";
                this->names_idx++;
                cv.notify_all();
            }
        }
        return ret_buf;
    }
};

#endif
