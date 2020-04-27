#ifndef _CONNECTOR_H_
#define _CONNECTOR_H_


#include <helpers/util.h>

#include <IO-config.h>
#include <IO/receiver.h>

// leave 0 for error checking as in forgetting to set but calloc should be
// noticed...
#define NET_IDX   1
#define STDIN_IDX 2

//#define HAS_STDIN

typedef struct connector {
    const char * ip_addr;
    uint32_t     portno;

    receiver_t * net_recvr;
#ifdef HAS_STDIN
    receiver_t * stdin_recvr;
#endif
    struct event_base * connect_base;
} connector_t;


connector_t * init_connector(const char * ip_addr, uint32_t portno);
void          free_connector(connector_t * connect);
void          start_connect(connector_t * connect);

#endif
