#include <IO/connector.h>

#ifdef HAS_STDIN
static void *
handle_stdin_command(void * arg, io_data * data_buf) {
    receiver_t * stdin_recvr = (receiver_t *)arg;
    int32_t net_fd = ((connector_t *)(stdin_recvr->my_parent))->net_recvr->fd;

    HEADER_TYPE hdr = (stdin_recvr->amt_read) + HEADER_SIZE + sizeof(char);
    char        null_term = 0;

    uint32_t ret = myrobustwrite(net_fd, (void *)(&hdr), HEADER_SIZE);
    ret += myrobustwrite(net_fd, stdin_recvr->buf, stdin_recvr->amt_read);
    ret += myrobustwrite(net_fd, &null_term, sizeof(char));

    DBG_ASSERT(hdr == ret,
               "Error didnt write expected bytes: %d vs %d\n",
               ret,
               hdr);

    stdin_recvr->amt_read = 0;
    stdin_recvr->rd_state = READING_NONE;
    return NULL;
}
#endif

static int32_t
do_connect(const char * ip_addr, uint32_t portno) {
    struct sockaddr_in addr;
    int                net_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port   = htons(portno);

    inet_aton(ip_addr, &addr.sin_addr);

    if (connect(net_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) ==
        (-1)) {
        errdie(
            "Couldnt connect to server at\n\t"
            "ip  : %s\n\t"
            "port: %d\n",
            ip_addr,
            portno);
    }
    return net_fd;
}


static void *
handle_command(void * arg, io_data * data_buf) {
    myfree(data_buf);
    return NULL;
}


connector_t *
init_connector(const char * ip_addr, uint32_t portno) {

    connector_t * new_connect = (connector_t *)mycalloc(1, sizeof(connector_t));

    new_connect->ip_addr = ip_addr;
    new_connect->portno  = portno;

    new_connect->connect_base = event_base_new();
    if (new_connect->connect_base == NULL) {
        errdie("Couldn't initialize connector event base\n");
    }

    int32_t net_fd         = do_connect(ip_addr, portno);
    new_connect->net_recvr = init_recvr(net_fd,
                                        0,
                                        NET_IDX,
                                        (void *)new_connect,
                                        new_connect->connect_base,
                                        &handle_command);
#ifdef HAS_STDIN
    new_connect->stdin_recvr = init_stdin_recvr(0,
                                                (void *)new_connect,
                                                new_connect->connect_base,
                                                &handle_stdin_command);
#endif

    return new_connect;
}

void
free_connector(connector_t * connect) {
    if (event_base_loopbreak(connect->connect_base) == (-1)) {
        errdie("Error breaking connector event loop\n");
    }
    free_recvr(connect->net_recvr);
#ifdef HAS_STDIN
    free_recvr(connect->stdin_recvr);
#endif
    myfree(connect);
}

void
start_connect(connector_t * connect) {
    if (event_base_loop(connect->connect_base, EVLOOP_NO_EXIT_ON_EMPTY) != 0) {
        errdie("Error running connector event loop\n");
    }
}
