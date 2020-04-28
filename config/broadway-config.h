#ifndef _BROADWAY_CONFIG_H_
#define _BROADWAY_CONFIG_H_

#define TYPE_TYPE char
#define TYPE_SIZE sizeof(char)


#define NTYPES 4
#define TYPE_IDX 0
#define DATA_START_IDX 1



#define KILL_MSG    0x1
#define AVAIL_MSG   0x2
#define CONTENT_MSG 0x3
#define CONTROL_MSG 0x4

#define IS_VALID_TYPE(X) (((X) >= 1) && ((X) <= NTYPES))


#define READY     0
#define CANCELLED 1
#define ONGOING   2

#define SHUTDOWN 3
#endif
