#ifndef _TEMP_H_
#define _TEMP_H_


//////////////////////////////////////////////////////////////////////
#include <general-config.h>
#include <helpers/util.h>
#include <local/core.h>
//////////////////////////////////////////////////////////////////////


#define NOT_FOUND 0  // Return value if the
// directory containing
// temperature files
// was not found


#define FOUND_DIR 1  // Return value if the
// directory containing
// temperature files
// was found

#define FOUND_ZONE 2  // Return value if the
// zone containing
// temperature files
// was found


#define SCALE_TO_C 1000  // Scale between file
// content and Celcius
//(i.e file 48000 ->
// 48 degrees C)

// opens all temp files we are tracking as indicated by
// cpu_set_t. Returns array of file descriptors mapped to the open
// temperature files. The array is dynamically allocated.
int32_t * getTempFiles(int32_t ncores,  // Number of cores to
                                        // to get files for

                       cpu_set_t * cpus  // cpu_set_t specifying
                       // which which cores'
                       // temperature files
                       // will be opened
);

// returns dynamically allocated array of temps read from each fd in
// fds
float * readNStore(int32_t ncores,  // number of cores in
                                    // fds array

                   int32_t * fds  // array of open file
                   // descriptors to core
                   // temperature files
);

// reads temp for a given fd
float getTemp(int32_t fd  // file descriptors to
                          // read from
);


// finds base path for where temp files can be found returns FOUND if
// was able to find coretemp directory returns NOT_FOUND if it was not
// able to find coretemp directory. Uses the recursive strategy
// described above to find the directory.
int32_t createTempBasePath(char * outstr  // optional argument if
                                          // you want to copy the
                                          // path to another
                                          // string (current used
                                          // for creating
                                          // compile-config.h
);


#endif
