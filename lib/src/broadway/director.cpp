#include <broadway/director.h>


static void
priority_kill_msg(receiver_t * recvr) {
    // we are never giving up this lock as director is exiting...
    ACQUIRE_OUTBUF(recvr->outbuf);
    recvr->outbuf_cur_size = 0;

    TYPE_TYPE   type = KILL_MSG;
    HEADER_TYPE hdr  = HEADER_SIZE + TYPE_SIZE;
    // ignore locks we already have
    store_recvr_outbuf(recvr, (uint8_t *)(&hdr), HEADER_SIZE, 0);
    store_recvr_outbuf(recvr, (uint8_t *)(&type), TYPE_SIZE, 0);


    // force this write to go through...
    make_blocking(recvr->fd);
    myrobustwrite(recvr->fd, OUTBUF_PTR(recvr->outbuf), hdr);

    fprintf(stderr, "Priority message sent\n");
    // dont unset lock. no more messages should be sent
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

static void
error_cancel_req(receiver_t * recvr) {
    char error_msg[SMALL_BUF_LEN] = "No In Progress Play\0";
    send_data(recvr,
              CONTROL_MSG,
              strlen("No In Progress Play") + HEADER_SIZE + TYPE_SIZE + 1,
              error_msg);
}

static void
error_play_req(receiver_t * recvr, io_data * data_buf) {
    uint32_t error_msg_len = strlen("Invalid Play Name: ") +
                             (data_buf->length - (HEADER_SIZE + TYPE_SIZE));

    char error_msg[BIG_BUF_LEN] = "Invalid Play Name: ";

    memcpy(error_msg + strlen("Invalid Play Name: "),
           &(data_buf->data[DATA_START_IDX]),
           data_buf->length - (HEADER_SIZE + TYPE_SIZE));

    error_msg[error_msg_len] = 0;
    error_msg_len++;

    send_data(recvr,
              CONTROL_MSG,
              error_msg_len + HEADER_SIZE + TYPE_SIZE,
              error_msg);
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

    if (this_director->shutdown) {
        return NULL;
    }
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

        // this is blocking so ~Director will not go through until ack sent
        priority_kill_msg(recvr);
        this_director->~Director();
    }
    else if (type == AVAIL_MSG) {
        // in case of cancelled or completed the last player thread will set
        // progress state to ready. Last recognized is only set here so the two
        // will not be equal and last_idx will need to be updated.
        if (this_director->in_progress != this_director->last_recognized) {
            this_director->update_status_idx(this_director->last_play_idx);
            this_director->last_recognized = this_director->in_progress;
        }

        // send back current status of plays
        this_director->send_avails();
    }
    else if (type == CONTENT_MSG) {
        std::string req_as_str =
            std::string((char *)(&(data_buf->data[DATA_START_IDX])));

        int32_t play_idx = this_director->find_req_play(req_as_str);
        if (play_idx >= 0) {
            this_director->in_progress     = ONGOING;
            this_director->last_recognized = ONGOING;
            this_director->last_play_idx   = 0;

            // if want content progress/lastrecognized are now ONGOING
            // update index, not sure exactly what to do if invalid
            DBG_ASSERT(this_director->update_status_idx(0) == 0,
                       "Error somehow found playname but not index...\n");


            // send avails to show ongoing
            this_director->send_avails();

            // start the play
            this_director->cue_idx(0);
        }
        else {
            this_director->in_progress     = READY;
            this_director->last_recognized = READY;

            error_play_req(recvr, data_buf);

            this_director->send_avails();
        }
    }

    // cancell request will come from control_msg
    else if (type == CONTROL_MSG) {
        if (this_director->in_progress != ONGOING) {
            error_cancel_req(recvr);
        }
        else {
            // by definition will be cancell (only control message there is)
            this_director->in_progress     = CANCELLED;
            this_director->last_recognized = CANCELLED;

            // update state with cancelled
            this_director->update_status_idx(this_director->last_play_idx);

            // send message to show was cancelled
            this_director->send_avails();
        }
    }
    else {
        die("Error this should absolutely never happen\n");
    }

    PRINT(HIGH_VERBOSE, "Exiting command handler\n");
    return NULL;
}


Director::Director(const string & name, char * ip_addr, uint32_t portno)
    : Director(name, 0, ip_addr, portno) {}

Director::Director(const string & name,
                   uint32_t       min_players,
                   char *         ip_addr,
                   uint32_t       portno) {

    std::istringstream space_iter(name);
    uint32_t           iter_num = 0, max = 0;
    for (std::string name; space_iter >> name;) {
        PRINT(HIGH_VERBOSE, "FILE: %s\n", name.c_str());
        this->files.push_back(ifstream(name));
        if (!this->files[iter_num].is_open()) {
            throw std::runtime_error("couldn't open file: " + name + " \n");
        }

        this->shutdown        = 0;
        this->in_progress     = READY;
        this->last_recognized = READY;
        this->last_play_idx   = 0;

        this->titles.push_back(name);
        this->statuses.push_back("Ready");


        string   buf;
        uint32_t prev = 0, scene = 0;
        uint32_t total_chars = 0;
        this->pconf.push_back(play_config_info());
        while (getline(this->files[iter_num], buf)) {
            // case of new scene
            if (strncmp(buf.c_str(), "[scene] ", strlen("[scene] ")) == 0) {

                this->pconf[iter_num].names.push_back(
                    buf.substr(strlen("[scene] ")));
                scene = 1;
            }
            else {

                // otherwise we expect a config file
                if (!scene) {
                    this->pconf[iter_num].names.push_back(
                        "");  // case where multiple nameless scenes
                              // in one scene
                    assert(this->pconf[iter_num].names.size());
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
                        //                        max  = MAX(max, prev +
                        //                        n_chars);
                        prev = n_chars;
                    }
                    else {
                        max = n_chars;
                    }
                    this->pconf[iter_num].configs.push_back(ifstream(
                        buf));  // stores config file names for this scene
                    this->pconf[iter_num].n_players.push_back(n_chars);
                    total_chars += n_chars;
                }
                scene = 0;
            }
        }


        this->nparts.push_back(total_chars);
        max = MAX(max, total_chars);

        this->p.push_back(make_shared<Play>(this->pconf[iter_num].names));
        if (!this->p[iter_num]) {
            throw std::runtime_error("Allocation error");
        }

        iter_num++;
    }
    // creates play/players based on scene file

    for (uint32_t j = 0; j < MAX(max, min_players); j++) {
        players.push_back(make_shared<Player>());
        players[j]->enter(this->frag_que, this->cv);
    }

    this->connect                       = init_connector(ip_addr, portno);
    this->connect->net_recvr->rd_handle = handle_net_cmd;
    set_owner(this->connect->net_recvr, (void *)this);

    this->cue_enter();
}


Director::~Director() {
    this->shutdown      = SHUTDOWN;
    this->frag_que.done = SHUTDOWN;

    // producer sent kill so really its poor usage if another content request
    // was made but just to be safe set cancel on both ends
    this->in_progress = CANCELLED;
    free_connector(this->connect);
    this->in_progress = CANCELLED;
    // complete all fragments set que to done and notify all worker
    // threads so they can exit properly
    frag_que.done = SHUTDOWN;
    frag_que.cv.notify_all();
    for (uint32_t j = 0; j < players.size(); j++) {
        fprintf(stderr, "Joined %d/%ld\n", j, players.size());
        while (!players[j]->exit()) {
        }
    }
    fprintf(stderr, "Joining cue\n");
    while (!this->cue_exit()) {
    }
}

void
Director::cue_idx(uint32_t idx) {
    this->last_play_idx = idx;
    this->cv_cue.notify_all();
}

// startup a new player thread
void
Director::cue_enter() {
    thread th([&] { this->cue_wrapper(); });
    this->t_cue = std::move(th);
}

bool
Director::cue_exit() {
    this->cv_cue.notify_all();
    if (this->t_cue.joinable()) {
        t_cue.join();
        return true;
    }
    return false;
}

void
Director::cue_wrapper() {
    unique_lock<mutex> lock(m_cue);
    while (!this->shutdown) {
        cv_cue.wait(lock);
        if (this->shutdown) {
            return;
        }
        cue(this->last_play_idx);
    }
}
void
Director::cue(uint32_t idx) {
    // iterates through scene fragments and coordinates players/play
    // based on current/next fragment
    fprintf(stderr, "Starting Cue(%d)!\n", idx);
    unique_lock<mutex> lock(m);
    this->p[idx]->reset_play_state();
    string * agr_outbuf       = new string;
    int32_t * volatile nfrags = (int32_t *)mymalloc(sizeof(int32_t));

    (*agr_outbuf) += CONTENT_MSG;
    *nfrags = this->nparts[idx];

    for (size_t frag = 0; frag < this->pconf[idx].configs.size() + 1; frag++) {
        cv.wait(lock, [&] {
            return ((!(this->p[idx]->get_on_stage() + frag_que.size())) ||
                    this->shutdown);
        });
        if (this->shutdown) {
            return;
        }

        if (frag == this->pconf[idx].configs.size()) {
            fprintf(stderr, "Breaking\n");
            break;
        }
        fprintf(stderr,
                "1: Loop %lu/%lu\n",
                frag,
                this->pconf[idx].configs.size());
        // reset play counter between fragments. This resets error
        // checking for missing lines, prints new scene if needed/does
        // any other book keeping

        this->p[idx]->reset_counter((agr_outbuf));
        // set number of expected players for checking skipped line for
        // given fragment
#ifdef TEST_CANCEL
        sleep(1);
#endif
        fprintf(stderr,
                "2: Loop %lu/%lu\n",
                frag,
                this->pconf[idx].configs.size());
        this->p[idx]->set_on_stage(this->pconf[idx].n_players[frag]);

        p_info pi;
        pi.agr_outbuf     = agr_outbuf;
        pi.frag_num       = frag;
        pi.progress_state = (int32_t * volatile) & this->in_progress;
        pi.recvr          = this->connect->net_recvr;
        pi.frags_left     = nfrags;
        pi.active_play    = this->p[idx];
        PRINT(HIGH_VERBOSE, "Queing frame[%d] %ld\n", *pi.frags_left, frag);


        string buf;
        while (getline(this->pconf[idx].configs[frag], buf)) {
            if (istringstream(buf) >> pi.name >> pi.file) {
                fprintf(stderr,
                        "MARKERHIT START: %s: %s\n",
                        pi.name.c_str(),
                        pi.file.c_str());
                // initialize que for worker threads to read from
                frag_que.push(pi);
                fprintf(stderr,
                        "MARKERHIT START2: %s: %s\n",
                        pi.name.c_str(),
                        pi.file.c_str());
            }
        }
        fprintf(stderr,
                "MARKERHIT START3: %s: %s\n",
                pi.name.c_str(),
                pi.file.c_str());


        this->pconf[idx].configs[frag].clear();
        this->pconf[idx].configs[frag].seekg(0, ios::beg);
    }
    fprintf(stderr, "Exit Cue(%d)\n", idx);
}

string
Director::format_avails() {
    DBG_ASSERT(this->titles.size() == this->statuses.size(),
               "Error mismatched titles and statuses: %d != %d\n",
               this->titles.size(),
               this->statuses.size());

    string send_buf = "";
    for (uint32_t i = 0; i < this->titles.size(); i++) {
        fprintf(stderr,
                "%d/%ld: %s\n",
                i,
                this->titles.size(),
                this->titles[i].c_str());
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
Director::find_req_play(string req_play_name) {
    int32_t size_as_int = this->titles.size();
    for (int32_t i = 0; i < size_as_int; i++) {
        PRINT(HIGH_VERBOSE,
              "Comparing %s vs %s -> %d\n",
              req_play_name.c_str(),
              this->titles[i].c_str(),
              !strcmp(req_play_name.c_str(), this->titles[i].c_str()));
        if (this->titles[i] == req_play_name) {
            return i;
        }
    }
    return (-1);
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
