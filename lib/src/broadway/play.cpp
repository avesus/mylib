#include <broadway/play.h>

using std::endl;

// recites the play. Coordinates all players via line_counter and
// within a given line via n_passed and on_stage.
void
Play::recite(set<line>::iterator & it,
             int32_t               expec_fragments_lines,
             int32_t * volatile progress_state,
             string * agr_outbuf) {

    fprintf(stderr, "Start Recite\n");
    unique_lock<mutex> l(m);

    string ret;
    while ((line_counter < it->linen &&  // case where potentiall good to recite
            this->scene_fragment_counter == expec_fragments_lines) ||
           (this->scene_fragment_counter < expec_fragments_lines)) {

        if((*(progress_state)) == CANCELLED) {
            fprintf(stderr, "End Recite\n");
            return;
        }
        
        n_passed++;


        // case where no player for the current line
        if (n_passed == on_stage) {
            // nobody could read this line, need to skip it
            (*agr_outbuf) += "\n";
            (*agr_outbuf) += "****** line ";
            (*agr_outbuf) += to_string(line_counter);
            (*agr_outbuf) += " skipped ******";

            line_counter++;
            n_passed = 0;
            cv.notify_all();
            fprintf(stderr, "End Recite\n");
            continue;
        }
        cv.wait(l);
        if((*(progress_state)) == CANCELLED) {
            fprintf(stderr, "End Recite\n");
            return;
        }
    }

    // error case
    if (line_counter != it->linen ||
        this->scene_fragment_counter > expec_fragments_lines) {


        /*        (*agr_outbuf) += "\n";
        (*agr_outbuf) += "****** line ";
        (*agr_outbuf) += to_string(it->linen);
        (*agr_outbuf) += " said by ";
        (*agr_outbuf) += it->character;
        (*agr_outbuf) += " skipped fragment ******";*/

        this->names_idx++;
        cv.notify_all();
        fprintf(stderr, "End Recite\n");
        return;
    }

    if((*(progress_state)) == CANCELLED) {
        fprintf(stderr, "End Recite\n");
            return;
        }
    // change of character
    if (cur_char != it->character) {
        if (cur_char != "") {
            (*agr_outbuf) += "\n\n";
        }
        cur_char = it->character;
        (*agr_outbuf) += it->character;
        (*agr_outbuf) += '.';
    }

    // actually printing the line
    (*agr_outbuf) += "\n";
    (*agr_outbuf) += to_string(it->linen);
    (*agr_outbuf) += ": ";
    (*agr_outbuf) += it->msg;

    line_counter++;
    n_passed = 0;
    cv.notify_all();
    fprintf(stderr, "End Recite\n");
}
