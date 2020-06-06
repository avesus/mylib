#include "driver.h"

int32_t verbose = 0;
int32_t rseed   = 0;

// clang-format off
#define Version "0.1"
static ArgOption args[] = {
  // Kind,        Method,		name,	    reqd,  variable,		help
  { KindOption,   Integer, 		"-v", 	    0,     &verbose, 		"Set verbosity level" },
  { KindOption,   Integer, 		"--seed", 	0,     &rseed,  		"Set random number seed" },
  { KindHelp,     Help, 	"-h" },
  { KindEnd }
};
// clang-format on

static ArgDefs argp = { args, "Main", Version, NULL };

int
main(int argc, char ** argv) {
    progname = argv[0];

    srand(rseed);
    srandom(rseed);

    FDBG_INIT_DEBUGGER;

    ArgParser * ap = createArgumentParser(&argp);
    if (parseArguments(ap, argc, argv)) {
        die("Error parsing arguments");
    }
    freeCommandLine();
    freeArgumentParser(ap);

    // code goes here

    FDBG_FREE_DEBUGGER;
    return 0;
}
