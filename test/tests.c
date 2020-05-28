#include "tests.h"

#define TEST_ALIST

#ifdef TEST_ALIST
#include <datastruct/arrlist.h>
#endif

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


#define AL_TEST_SIZE (1 << 27)
static void test_alist();


int
main(int argc, char ** argv) {
    FDBG_INIT_DEBUGGER;
    
    progname = argv[0];

    srand(rseed);
    srandom(rseed);



    ArgParser * ap = createArgumentParser(&argp);
    if (parseArguments(ap, argc, argv)) {
        die("Error parsing arguments");
    }
    freeCommandLine();
    freeArgumentParser(ap);

#ifdef TEST_ALIST
    PRINT(LOW_VERBOSE, "Testing Array List with %d Test Size\n", AL_TEST_SIZE);
    test_alist();
#endif

    FDBG_FREE_DEBUGGER;
    return 0;
}


static void
test_alist() {

    int64_t  j     = 0;
    uint32_t count = 0;
    uint32_t temp  = 0;

    arr_list_t * alist = init_alist();
    assert(alist);

    PRINT(MED_VERBOSE, "Testing Add\n");
    for (j = 0; j < AL_TEST_SIZE; j++) {
        add_node(alist, (void *)j);
    }

    count = count_ll(alist);
    DBG_ASSERT(count == AL_TEST_SIZE,
               "Error: count_ll does not match (%d != %d)\n",
               count,
               AL_TEST_SIZE);

    j    = AL_TEST_SIZE - 1;
    temp = alist->ll;
    while (temp) {
        DBG_ASSERT(AL_TO_PTR(temp, alist->mem_addr)->data == (void *)j,
                   "Error: Data does not match (%p != %p)\n",
                   (void *)j,
                   AL_TO_PTR(temp, alist->mem_addr)->data);
        j--;
        temp = AL_TO_PTR(temp, alist->mem_addr)->next;
    }


    count = count_ll(alist);
    DBG_ASSERT(count == AL_TEST_SIZE,
               "Error: count_ll does not match (%d != %d)\n",
               count,
               AL_TEST_SIZE);

    PRINT(MED_VERBOSE, "Testing Get IDX\n");
    for (j = 0; j < AL_TEST_SIZE; j++) {
        arr_node_t * n = get_node_idx(alist, j);
        DBG_ASSERT(n, "NULL returned by get_node_idx(%p, %d)\n", alist, j);
        DBG_ASSERT(n->data == (void *)j,
                   "Data does not match: %p != %p\n",
                   n->data,
                   (void *)j);
    }

    DBG_ASSERT(alist->nitems == AL_TEST_SIZE,
               "Error: Invalid nitems: %d\n",
               alist->nitems);

    PRINT(MED_VERBOSE, "Testing Remove IDX\n");
    for (j = 0; j < AL_TEST_SIZE; j += 2) {
        remove_node_idx(alist, j);
    }

    assert(count_ll(alist) == AL_TEST_SIZE / 2);
    assert(count_que(alist) == AL_TEST_SIZE / 2);


    j    = AL_TEST_SIZE - 1;
    temp = alist->ll;
    while (temp) {

        DBG_ASSERT(AL_TO_PTR(temp, alist->mem_addr)->data == (void *)j,
                   "Error: Data does not match (%p != %p)\n",
                   (void *)j,
                   AL_TO_PTR(temp, alist->mem_addr)->data);
        j -= 2;
        temp = AL_TO_PTR(temp, alist->mem_addr)->next;
    }

    DBG_ASSERT(j == (-1), "Error: Invalid j value (%d)\n", j);

    assert(count_ll(alist) == AL_TEST_SIZE / 2);
    assert(count_que(alist) == AL_TEST_SIZE / 2);

    for (j = 0; j < AL_TEST_SIZE; j += 2) {
        assert(!get_node_idx(alist, j));
    }

    DBG_ASSERT(alist->nitems == AL_TEST_SIZE,
               "Error: Invalid nitems: %d\n",
               alist->nitems);

    PRINT(MED_VERBOSE, "Testing Free IDX que\n");
    count = 0;
    temp  = alist->free_idx_que;
    while (temp) {
        count++;
        temp = AL_TO_PTR(temp, alist->mem_addr)->next;
    }

    DBG_ASSERT(count == (AL_TEST_SIZE / 2),
               "Error: invalid number of free idx nodes (%d vs %d)\n",
               count,
               AL_TEST_SIZE / 2);


    PRINT(MED_VERBOSE, "Testing List\n");

    j    = AL_TEST_SIZE - 1;
    temp = alist->ll;
    while (temp) {
        DBG_ASSERT(AL_TO_PTR(temp, alist->mem_addr)->data == (void *)j,
                   "Error: Data does not match (%p != %p)\n",
                   (void *)j,
                   AL_TO_PTR(temp, alist->mem_addr)->data);
        j -= 2;
        temp = AL_TO_PTR(temp, alist->mem_addr)->next;
    }

    DBG_ASSERT(j == (-1), "Error: Invalid j value (%d)\n", j);

    PRINT(MED_VERBOSE, "Testing Re-add\n");
    for (j = 0; j < AL_TEST_SIZE; j += 2) {
        add_node(alist, (void *)j);
    }

    assert(alist->free_idx_que == 0);

    DBG_ASSERT(alist->nitems == AL_TEST_SIZE,
               "Error: Invalid nitems: %d\n",
               alist->nitems);

    PRINT(MED_VERBOSE, "Testing Remove All\n");
    count = 0;
    while (pop_node(alist)) {
        count++;
    }

    DBG_ASSERT(alist->nitems == count, "Error: Invalid llcount: %d\n", count);
    free_alist(alist);
}
