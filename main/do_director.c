#include "do_director.h"


#define ARG_PARSER_ON
#define ARG_PARSER_OFF

#if defined ARG_PARSER_ON && defined ARG_PARSER_OFF
    #undef ARG_PARSER_ON
#endif

#if !defined ARG_PARSER_ON && !defined ARG_PARSER_OFF
#define ARG_PARSER_OFF
#endif

#ifdef ARG_PARSER_OFF
    #define FILE_START_IDX 3
#endif


int32_t verbose = HIGH_VERBOSE;

#ifdef ARG_PARSER_ON


char * files = NULL;

    #define NSTRINGS 1
void * to_free[NSTRINGS];

    // clang-format off
#define Version "0.1"
static ArgOption args[] = {
  // Kind,        Method,		name,	    reqd,  variable,		help
  { KindOption,   Integer, 		"-v", 	    0,     &verbose, 		"Set verbosity level" },
  { KindOption,   String, 		"-f", 	    0,     &files,   		"Set play files, seperated by spaces" },
  { KindHelp,     Help, 	"-h" },
  { KindEnd }
};
// clang-format on

static ArgDefs argp = { args, "Main", Version, NULL };
#endif

int
main(int argc, char ** argv) {

    srand(argc);
    INIT_DEBUGGER;

#ifdef ARG_PARSER_ON
    files      = (char *)mycalloc(BIG_BUF_LEN, sizeof(char));
    to_free[0] = files;


    progname = argv[0];

    ArgParser * ap = createArgumentParser(&argp);
    if (parseArguments(ap, argc, argv)) {
        errdie("Error parsing arguments");
    }
    freeCommandLine();
    freeArgumentParser(ap);
    std::string str_files = files;
#endif
#ifdef ARG_PARSER_OFF
    std::string str_files = argv[FILE_START_IDX];
    for (int32_t i = FILE_START_IDX + 1; i < argc; i++) {
        str_files += " ";
        str_files += argv[i];
    }
#endif


    Director * d = new Director(str_files, LOCAL_IP, PORTNO);
    d->start_directing();


    FREE_DEBUGGER;

#ifdef ARG_PARSER_ON
    for (int32_t f = 0; f < NSTRINGS; f++) {
        myfree(to_free[f]);
    }
#endif
    return 0;
}
