#include "do_director.h"


#define CONNECT 1
#define ACCEPT  2

int32_t verbose = 0;

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

int
main(int argc, char ** argv) {
    files = (char *)mycalloc(MED_BUF_LEN, sizeof(char));
    to_free[0] = files;
    
    srand(argc);
    INIT_DEBUGGER;
    progname = argv[0];

    ArgParser * ap = createArgumentParser(&argp);
    if (parseArguments(ap, argc, argv)) {
        errdie("Error parsing arguments");
    }
    freeCommandLine();
    freeArgumentParser(ap);

    std::string str_files = files;
    Director * d = new Director(str_files, LOCAL_IP, PORTNO);
    d->start_directing();


    FREE_DEBUGGER;
    for(int32_t f = 0; f < NSTRINGS; f++) {
        myfree(to_free[f]);
    }
    return 0;
}
