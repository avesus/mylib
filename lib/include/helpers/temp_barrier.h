#ifndef _TEMP_BARRIER_H_
#define _TEMP_BARRIER_H_

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////
#include <general-config.h>
#include <helpers/temp.h>
#include <helpers/util.h>
//////////////////////////////////////////////////////////////////////

// package to monitor temperature of cores in a multicore processor.
// Specific to linux.  Tested on Ubuntu 18.04 & Ubuntu 16.04.


#define TRIALS_GUESS 32  // for initial allocation if
// STORE_DATA is defined and
// num trials is not

#define MAX_SLEEPS_GUESS 256  // for initial allocation if
// STORE_DATA is defined and
// timeout_sec is not defined

#define STORE_DATA  // define to store data after
// each temperature checl

#define INTERNAL_ALLOC 0x1  // set in lower bit of
// init_temps if allocation was
// done in init (i.e init_temps
// was not set in attr)

#define local_get_ptr(X) ((float *)low_bits_get_ptr((X)))


#define MAINTAIN(X, Y)  ((uint64_t)(((double)(X)) / (Y)))
#define MIN_GRAN_FACTOR 100  // After consecutives
// sleeps/spinning will
// increase duration until
// MIN_GRAN_FACTOR
typedef enum enforcer_modes {
    BELOW_THRESH = 0,
    WITHIN_RANGE = 1
} enforcer_modes;


// temp barrier struct. Note that while this is meant to coordinate
// multiple threads. Most of the functionality is meant to be called by
// a single thread
typedef struct temp_barrier {
    cpu_set_t * cpus;  // cpus that are being regulated

    int32_t ncores;  // number of cpus that are
    // being regulated. Not a
    // necessary argument but
    // CPU_COUNT() is not free and
    // it aligns with the next
    // float


    float allowed_variance;  // allowed variance to reach
    // required cooldown. I.e if
    // init temperature is c, to
    // meet threshold current temp
    // must be bellow c *
    // allowed_variance

    enforcer_modes enf_mode;  // enforcer mode, either
    // ensures within range (i.e if
    // temp drops to much will
    // increase) or just ensures
    // falls below threshold

    float * init_temps;  // init temperature for
    // cooldown threshold

    int32_t * fds;  // file descriptors to
    // temperature files for each
    // cpu being regulated


    int32_t usec_sleep_time;  // sleep time between reaching
    // if temperature threshold has
    // been met

    int32_t timeout_sec;  // timeout in seconds. If
    // temperatures have not
    // dropped below threshold
    //(cooled off) by timeout
    // error occurs

    // Todo fix this stuff
#ifdef STORE_DATA
    struct timespec ** timestamps;
    float ***          temps;  // this is by definition not on a critical path so
    // will just live with amortized O(N)
    int32_t * index;  // lengths[i] stores amount of temps collected at
    // temps[i]
    int32_t * bounds;  // bounds[i] stores current allocated space for temps[i]
    int32_t trial_bound;  // current max trials allocated
    int32_t cur_trial;    // cur trial number we are collecting for
#endif

    pthread_barrier_t barrier;  // actual barrier that all this
    // temperature bullshit wraps
} temp_barrier;

#define NO_TIMEOUT (~0U)

// default values for temp_barrier_attr fields.
#define DEFAULT_TIMEOUT (NO_TIMEOUT)  // default is no timeout

#define DEFAULT_TRIALS (-1)  // if store data no trial max

#define DEFAULT_SLEEP us_per_sec  // default sleep time is once
// per sec

#define DEFAULT_VARIANCE 1.1  // default variance is 1.1

#define DEFAULT_ENF_MODE BELOW_THRESH  // default enforcer mode

// attributes struct is for setting various fields in
// temp_barrier. This is entirely for a cleaner api. Variable names in
// attr correspond 1-1 with variable names in temp_barrier
typedef struct temp_barrier_attr {
    cpu_set_t * cpus;             // to set cpus
    int32_t     usec_sleep_time;  // to set sleep time
    uint32_t    timeout_sec;      // to set timeout
    float allowed_variance;       // to set allowed_variance
    float *        init_temps;    // to set init_temps
    enforcer_modes enf_mode;      // to set enforcer mode
    // Todo fix this stuff
#ifdef STORE_DATA
    int32_t ntrials;
#endif
} temp_barrier_attr;

// initialize attr struct with reasonable values
void initAttr(temp_barrier_attr * attr  // attr struct to be
                                        // initialized
);


// This set of functions initializes a specific field in an
// temp_barrier_attr struct. I bet you can guess which one each
// function initializes
void attrSetSleepUS(temp_barrier_attr * attr, int32_t usec_sleep_time);
void attrSetTimeoutSec(temp_barrier_attr * attr, uint32_t timeout_sec);
void attrSetAllowedVariance(temp_barrier_attr * attr, float allowed_variance);
void attrSetInitTemps(temp_barrier_attr * attr, float * init_temps);
void attrSetTrials(temp_barrier_attr * attr, int32_t ntrials);
void attrSetEnforcer(temp_barrier_attr * attr, enforcer_modes mode);
void attrSetCPUS(temp_barrier_attr * attr, cpu_set_t * cpus);


// initialize temperature barrier and returns a point32_ter to a
// dynamically allocated temp_barrier struct
temp_barrier * initTempBarrier(int32_t barrier_num,  // barrier num
                                                     // for
                                                     // initializing
                                                     // the
                                                     // pthread_barrier_t

                               temp_barrier_attr * attr  // attr for
                               // initializing
                               // various
                               // fields in
                               // the struct
);


// called to first have all threads wait at the barrier then have all
// threads wait until each core has cooled off. Coordinator should be
// set by a single thread that will actually call the int32_ternal
// temperature regulation functions.
void tempBarrierWait(temp_barrier * tmp_barrier,  // barrier

                     int32_t coordinator  // boolean that
                     // should only
                     // be set by
                     // one thread
);

// frees all memory
void freeTempBarrier(temp_barrier * tmp_barrier  // barrier that
                                                 // will have
                                                 // all the
                                                 // dynamic
                                                 // memory
                                                 // assosiated
                                                 // with it free
                                                 //(this
                                                 // includes
                                                 // freeing
                                                 // tmp_barrier)
);

// print temp barrier data, really does nothing if STORE_DATA is not
// defined
void printBarrierData(temp_barrier * tmp_barrier,  // barrier to
                                                   // print data
                                                   // from

                      FILE * outfile  // file to
                      // write data
                      // to
);
#endif
