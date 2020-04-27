#include "do_producer.h"


#define CONNECT 1
#define ACCEPT  2

int32_t verbose = 0;

// clang-format off
#define Version "0.1"
static ArgOption args[] = {
  // Kind,        Method,		name,	    reqd,  variable,		help
  { KindOption,   Integer, 		"-v", 	    0,     &verbose, 		"Set verbosity level" },
  { KindHelp,     Help, 	"-h" },
  { KindEnd }
};
// clang-format on

static ArgDefs argp = { args, "Main", Version, NULL };

int
main(int argc, char ** argv) {
    srand(argc);
    INIT_DEBUGGER;
    progname = argv[0];

    ArgParser * ap = createArgumentParser(&argp);
    if (parseArguments(ap, argc, argv)) {
        errdie("Error parsing arguments");
    }
    freeCommandLine();
    freeArgumentParser(ap);

    producer * p = new producer(0, LOCAL_IP, PORTNO);
    p->produce();


    FREE_DEBUGGER;
    return 0;
}
