#ifndef _ARR_LIST_H_
#define _ARR_LIST_H_

#include <general-config.h>

#include <helpers/debug.h>
#include <helpers/opt.h>
#include <helpers/util.h>


#define DEFAULT_INIT_AL_SIZE (1 << 30)
#define AL_TO_PTR(X, Y)      ((arr_node_t *)(((uint64_t)(X)) + (Y)))

typedef struct arr_node {
    uint32_t next;
    uint32_t prev;
    void *   data;
} arr_node_t;

typedef struct arr_list {
    uint64_t mem_addr;
    uint32_t ll;
    uint32_t free_idx_que;
    uint32_t nitems;
} arr_list_t;


arr_list_t * init_alist();
void         free_alist(arr_list_t * a_list);

void remove_node_idx(arr_list_t * alist, uint32_t idx);
void remove_node(arr_list_t * alist, arr_node_t * drop_node);

arr_node_t * add_node(arr_list_t * alist, void * data);
arr_node_t * get_node_idx(arr_list_t * alist, uint32_t idx);

arr_node_t * get_next(arr_list_t * alist, arr_node_t * node);
arr_node_t * get_prev(arr_list_t * alist, arr_node_t * node);

arr_node_t * pop_node(arr_list_t * alist);

uint32_t count_que(arr_list_t * alist);
uint32_t count_ll(arr_list_t * alist);
#endif
