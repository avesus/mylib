#include <broadway/play.h>

using std::endl;

// recites the play. Coordinates all players via line_counter and
// within a given line via n_passed and on_stage.
void
Play::recite(set<line>::iterator & it,
             int32_t               expec_fragments_lines,
             string &              agr_outbuf) {
    unique_lock<mutex> l(m);
    string             ret;
    while ((line_counter < it->linen &&  // case where potentiall good to recite
            this->scene_fragment_counter == expec_fragments_lines) ||
           (this->scene_fragment_counter < expec_fragments_lines)) {

        n_passed++;

        fprintf(stderr,
                "handling line: %d[%s] -> %s\n",
                it->linen,
                it->character.c_str(),
                it->msg.c_str());

        // case where no player for the current line
        if (n_passed == on_stage) {
            fprintf(stderr, "Case 1(%d)\n", it->linen);
            // nobody could read this line, need to skip it
            agr_outbuf += "\n";
            agr_outbuf += "****** line ";
            agr_outbuf += to_string(line_counter);
            agr_outbuf += " skipped ******";

            line_counter++;
            n_passed = 0;
            cv.notify_all();
            continue;
        }
        cv.wait(l);
    }
    // error case
    if (line_counter != it->linen ||
        this->scene_fragment_counter > expec_fragments_lines) {
        fprintf(stderr,
                "Case 2(%d) [%d != %d || %d > %d]\n",
                it->linen,
                line_counter,
                it->linen,
                this->scene_fragment_counter,
                expec_fragments_lines);
        
        /*        agr_outbuf += "\n";
        agr_outbuf += "****** line ";
        agr_outbuf += to_string(it->linen);
        agr_outbuf += " said by ";
        agr_outbuf += it->character;
        agr_outbuf += " skipped fragment ******";*/

        this->it++;
        cv.notify_all();
        return;
    }

    // change of character
    if (cur_char != it->character) {
        fprintf(stderr, "Case 3(%d)\n", it->linen);
        if (cur_char != "") {
            agr_outbuf += "\n\n";
        }
        cur_char = it->character;
        agr_outbuf += it->character;
        agr_outbuf += '.';
    }

    fprintf(stderr, "Case 4(%d)\n", it->linen);
    // actually printing the line
    agr_outbuf += "\n";
    agr_outbuf += to_string(it->linen);
    agr_outbuf += ": ";
    agr_outbuf += it->msg;

    line_counter++;
    n_passed = 0;
    cv.notify_all();
}
