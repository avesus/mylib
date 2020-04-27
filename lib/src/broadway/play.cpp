#include <broadway/play.h>

using std::endl;

// recites the play. Coordinates all players via line_counter and
// within a given line via n_passed and on_stage.
string
Play::recite(set<line>::iterator & it, int32_t expec_fragments_lines) {
    unique_lock<mutex> l(m);
    string             ret;
    while ((line_counter < it->linen &&  // case where potentiall good to recite
            this->scene_fragment_counter == expec_fragments_lines) ||
           (this->scene_fragment_counter < expec_fragments_lines)) {

        n_passed++;


        // case where no player for the current line
        if (n_passed == on_stage) {
            // nobody could read this line, need to skip it
            ret += "\n";
            ret += "****** line ";
            ret += line_counter;
            ret += " skipped ******";

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
        ret += "\n";
        ret += "****** line ";
        ret += it->linen;
        ret += " said by ";
        ret += it->character;
        ret += " skipped fragment ******";

        this->it++;
        cv.notify_all();
        return ret;
    }

    // change of character
    if (cur_char != it->character) {
        if (cur_char != "") {
            ret += "\n\n";
        }
        cur_char = it->character;
        ret += it->character;
        ret += '.';
    }

    // actually printing the line
    ret += "\n";
    ret += it->linen;
    ret += ": ";
    ret += it->msg;

    line_counter++;
    n_passed = 0;
    cv.notify_all();
    return ret;
}
