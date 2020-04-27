#include <broadway/director.h>


static void
send_type(receiver_t * recvr, TYPE_TYPE type) {
    HEADER_TYPE hdr = HEADER_SIZE + TYPE_SIZE;

    PRINT(HIGH_VERBOSE, "Sending[%d] new type [%d]\n", hdr, type);

    store_recvr_outbuf(recvr, (uint8_t *)(&hdr), HEADER_SIZE, ACQUIRE);
    store_recvr_outbuf(recvr, (uint8_t *)(&type), TYPE_SIZE, RELEASE);

    reset_recvr_event(recvr, &handle_event, EV_WRITE, WRITING);
}

static void
send_data(receiver_t * recvr, TYPE_TYPE type, HEADER_TYPE hdr, char * data) {

    // assume header == 0 means needs to set header
    if (hdr == 0) {
        hdr = HEADER_SIZE + TYPE_SIZE + strlen(data);
    }

    DBG_ASSERT(IS_VALID_TYPE(type), "Error invalid type: %d\n", type);

    PRINT(LOW_VERBOSE, "writing: %s\n", data);
    store_recvr_outbuf(recvr, (uint8_t *)(&hdr), HEADER_SIZE, ACQUIRE);
    store_recvr_outbuf(recvr, (uint8_t *)(&type), TYPE_SIZE, 0);
    store_recvr_outbuf(recvr,
                       (uint8_t *)data,
                       (hdr - (HEADER_SIZE + TYPE_SIZE)),
                       RELEASE);

    reset_recvr_event(recvr, &handle_event, EV_WRITE, WRITING);
}


static void *
handle_net_cmd(void * arg, io_data * data_buf) {
    DBG_ASSERT(data_buf != NULL && arg != NULL,
               "In net handler with invalid params: %p, %p\n",
               arg,
               data_buf);

    receiver_t * recvr         = (receiver_t *)arg;
    Director *   this_director = (Director *)(recvr->my_owner);
    TYPE_TYPE    type          = data_buf->data[TYPE_IDX];

    DBG_ASSERT(IS_VALID_TYPE(type), "Error invalid type: %d\n", type);

    PRINT(HIGH_VERBOSE,
          "Recived new command length(%d), type(%d)\n",
          data_buf->length,
          type);
    // director kill will basically always originate from producer. Director
    // kill response is verification
    if (type == KILL_MSG) {
        DBG_ASSERT(data_buf->length == HEADER_SIZE + TYPE_SIZE,
                   "Error invalid length(%d) for KILL_MSG\n",
                   data_buf->length);

        send_type(recvr, KILL_MSG);
        this_director->~Director();
    }
    else if (type == AVAIL_MSG) {
        // in case where play completed normally need to update for avail
        // message
        if (this_director->in_progress != this_director->last_recognized) {
            this_director->update_status_idx(this_director->last_play_idx);
            this_director->last_recognized = this_director->in_progress;
        }

        this_director->send_avails();
        // send back current status of plays
    }
    else if (type == CONTENT_MSG) {

        // if want content progress/lastrecognized are now ONGOING
        this_director->in_progress     = ONGOING;
        this_director->last_recognized = ONGOING;
        this_director->last_play_idx   = 0;

        // update index, not sure exactly what to do if invalid
        if (this_director->update_status_idx(0) == 0) {

            // send avails to show ongoing
            this_director->send_avails();

            // start the play
            this_director->cue();
        }
        else {
            fprintf(stderr, "Noah remember to put a control message here\n");
        }
    }

    // cancell request will come from control_msg
    else if (type == CONTROL_MSG) {

        // by definition will be cancell (only control message there is)
        this_director->in_progress     = CANCELLED;
        this_director->last_recognized = CANCELLED;

        //update state with cancelled
        this_director->update_status_idx(this_director->last_play_idx);

        //send message to show was cancelled
        this_director->send_avails();

        //reset state and send updated avail messages
        this_director->in_progress     = READY;
        this_director->last_recognized = READY;
        this_director->send_avails();
    }
    else {
        die("Error this should absolutely never happen\n");
    }


    return NULL;
}


Director::Director(const string & name, char * ip_addr, uint32_t portno)
    : Director(name, 0, ip_addr, portno) {}

Director::Director(const string & name,
                   uint32_t       min_players,
                   char *         ip_addr,
                   uint32_t       portno)
    : i(name) {

    if (!i.is_open()) {
        throw std::runtime_error("couldn't open file for reading");
    }

    this->in_progress     = READY;
    this->last_recognized = READY;
    this->last_play_idx   = 0;

    this->titles.push_back(name);
    this->statuses.push_back("Ready");


    string   buf;
    uint32_t max = 0, prev = 0, scene = 0;

    while (getline(i, buf)) {
        // case of new scene
        if (strncmp(buf.c_str(), "[scene] ", strlen("[scene] ")) == 0) {
            names.push_back(buf.substr(strlen("[scene] ")));
            scene = 1;
        }
        else {

            // otherwise we expect a config file
            if (!scene) {
                names.push_back("");  // case where multiple nameless scenes
                                      // in one scene
            }
            ifstream config(buf);
            if (config.is_open()) {
                int    n_chars = 0;
                string buf2;

                // get number of players for this scene
                while (getline(config, buf2)) {
                    n_chars++;
                }

                if (!scene) {
                    // there is a config file before this
                    max  = MAX(max, prev + n_chars);
                    prev = n_chars;
                }
                else {
                    max = n_chars;
                }
                configs.push_back(
                    ifstream(buf));  // stores config file names for this scene
                n_players.push_back(n_chars);
            }
            scene = 0;
        }
    }

    // creates play/players based on scene file
    p = make_shared<Play>(names);
    if (!p) {
        throw std::runtime_error("Allocation error");
    }
    for (uint32_t j = 0; j < MAX(max, min_players); j++) {
        players.push_back(make_shared<Player>(*p));
        players[j]->enter(this->frag_que, this->cv);
    }

    this->connect                       = init_connector(ip_addr, portno);
    this->connect->net_recvr->rd_handle = handle_net_cmd;
    set_owner(this->connect->net_recvr, (void *)this);
}


Director::~Director() {
    free_connector(this->connect);

    // complete all fragments set que to done and notify all worker
    // threads so they can exit properly
    frag_que.done = 1;
    frag_que.cv.notify_all();
    for (uint32_t j = 0; j < players.size(); j++) {
        players[j]->exit();
    }
}


void
Director::cue() {

    // iterates through scene fragments and coordinates players/play
    // based on current/next fragment
    unique_lock<mutex> lock(m);
    vector<p_info *>   p;
    for (size_t frag = 0; frag < configs.size() + 1; frag++) {
        cv.wait(lock,
                [&] { return !(this->p->get_on_stage() + frag_que.size()); });
        if (frag == configs.size()) {
            break;
        }
        p.push_back(new p_info());
        // reset play counter between fragments. This resets error
        // checking for missing lines, prints new scene if needed/does
        // any other book keeping
        this->p->reset_counter();

        // set number of expected players for checking skipped line for
        // given fragment

        this->p->set_on_stage(n_players[frag]);

        p[frag]->frag_num       = frag;
        p[frag]->progress_state = (int32_t * volatile) & this->in_progress;
        p[frag]->recvr          = this->connect->net_recvr;
        p[frag]->last_frag      = (frag == (configs.size() - 1));

        string buf;
        while (getline(this->configs[frag], buf)) {
            if (istringstream(buf) >> p[frag]->name >> p[frag]->file) {
                // initialize que for worker threads to read from
                frag_que.push(p[frag]);
            }
        }
    }
}

string
Director::format_avails() {
    DBG_ASSERT(this->titles.size() == this->statuses.size(),
               "Error mismatched titles and statuses: %d != %d\n",
               this->titles.size(),
               this->statuses.size());

    string send_buf = "";
    for (uint32_t i = 0; i < this->titles.size(); i++) {
        uint32_t start_len = send_buf.length();

        send_buf += std::to_string(i);
        for (uint32_t p = (send_buf.length() - start_len); p < INDEX_LEN;

             p++) {
            send_buf += " ";
        }
        send_buf += " - ";
        send_buf += this->statuses[i];
        send_buf += " - ";
        for (uint32_t p = (send_buf.length() - start_len); p < STATUS_LEN;
             p++) {
            send_buf += " ";
        }
        send_buf += this->titles[i];
        send_buf += "\n";
        if (i != (this->titles.size() - 1)) {
            send_buf += "\t";
        }
    }
    send_buf += "\0";

    return send_buf;
}

void
Director::send_avails() {
    std::string send_buf = this->format_avails();
    send_data(
        this->connect->net_recvr,
        AVAIL_MSG,
        send_buf.length() + 1 + HEADER_SIZE + TYPE_SIZE,  // + 1 for null term
        (char *)send_buf.c_str());
}


void
Director::start_directing() {
    start_connect(this->connect);
}


int32_t
Director::update_status_idx(uint32_t idx) {
    if (this->in_progress == READY) {
        for (uint32_t i = 0; i < this->statuses.size(); i++) {
            this->statuses[i] = "Ready";
        }
    }
    else {
        if (idx >= this->statuses.size()) {
            return (-1);
        }
        else if (this->in_progress == ONGOING) {
            this->statuses[idx] = "In Progress";
        }
        else if (this->in_progress == CANCELLED) {
            this->statuses[idx] = "Stopped";
        }
    }
    return 0;
}
