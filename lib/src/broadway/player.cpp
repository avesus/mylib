#include <broadway/player.h>


// reads config file and inserts all lines to players data structure
void
Player::read(string name, string file) {
    this->lines.clear();
    ifstream ifs(file);
    string   line_str;
    line     l;
    while (getline(ifs, line_str)) {

        istringstream is(line_str);
        if (!(is >> l.linen)) {
            // could not extract line number or first whitespace-delimited
            // string was not a positive number
            continue;
        }

        // need to ignore extra spaces after line number, so extract one
        // word from the line
        if (!(is >> l.msg)) {
            // line is empty
            continue;
        }
        // now get remaining text from the line

        if (!(is.eof())) {
            getline(is, line_str);
            l.msg += line_str;
        }

        l.character = name;
        lines.insert(l);
    }
}


// recites each of the lines for this fragment number of the player
void
Player::act(p_info pi) {
    for (auto it = lines.begin(); it != lines.end(); it++) {
        PRINT(HIGH_VERBOSE, "(%ld) Doing line Outside: %d[%s]: %s\n", pthread_self(),it->linen, it->character.c_str(), it->msg.c_str());
        if ((*(pi.progress_state)) == CANCELLED) {
            die("Currently unsupported\n");
            clear_recvr_outbuf(pi.recvr);
            p.player_exit(CANCELLED);
            break;
        }
        p.recite(it, pi.frag_num, pi.agr_outbuf);
    }

    fprintf(stderr, "MARKERHIT DONE\n");
    if ((*(pi.progress_state)) != CANCELLED) {
        (*(pi.progress_state)) = READY;
        p.player_exit(READY);
        if (__atomic_sub_fetch((pi.frags_left), 1, __ATOMIC_RELAXED) == 0) {
            DBG_ASSERT(pi.agr_outbuf[0] == CONTENT_MSG,
                       "Error type got corrupted %d -> %d\n",
                       CONTENT_MSG,
                       pi.agr_outbuf[0]);
            PRINT(HIGH_VERBOSE,
                  "Preparing to write: [%ld] ->\n%s\n",
                  HEADER_SIZE + pi.agr_outbuf.length(),
                  pi.agr_outbuf.c_str());

            prepare_send_recvr(pi.recvr,
                               HEADER_SIZE + pi.agr_outbuf.length(),
                               (uint8_t *)pi.agr_outbuf.c_str());

            reset_recvr_event(pi.recvr, &handle_event, EV_WRITE, WRITING);
        }
    }


    // exit when lines are done (as if there is no more to acting
    // than reciting lines... where is the expressiveness!)
}

// while !done (i.e directory finds last fragment is done) continue
// trying to pop parts of the que
void
Player::work(sync_que & q, condition_variable & cv_dir) {
    while (!q.done) {
        p_info p = q.pop();
        if (q.done) {
            break;
        }

        read(p.name, p.file);
        act(p);
        cv_dir.notify_all();
    }
}


// startup a new player thread
void
Player::enter(sync_que & q, condition_variable & cv_dir) {
    thread th([&] { this->work(q, cv_dir); });
    this->t = std::move(th);
}

// quit a player thread
bool
Player::exit() {
    if (this->t.joinable()) {
        t.join();
        return true;
    }
    return false;
}
