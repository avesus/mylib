#ifndef _ARR_LIST_H_
#define _ARR_LIST_H_

#include <general-config.h>

#include <helpers/debug.h>
#include <helpers/opt.h>
#include <helpers/util.h>

#define MAX_IDX_LEN        4
#define PRINT_PRO_BASE_LEN strlen("Director: {")
#define EPI_PRINT_OFFSET   (int32_t)(PRINT_PRO_BASE_LEN + MAX_IDX_LEN)

#define PRINT_FORMAT_STR_PROLOGUE "Director: %d%*c{\n\t"
#define PRINT_FORMAT_STR_EPILOGUE "%*c}\n"
#define DEFAULT_INIT_SIZE         64

#define INT32_MASK 0xffffffff

#define get_x_idx(X)        ((X)&INT32_MASK)
#define get_y_idx(X)        (((X) >> sizeof_bits(uint32_t)) & INT32_MASK)
#define x_y_to_idx(X, Y, Z) (((X) ? (1 << ((Z) + ((X)-1))) : 0) + (Y))


typedef struct arr_node {
    uint32_t          x_idx;
    uint32_t          y_idx;
    volatile void *   usr_data;
    void *            usr_key;
    struct arr_node * next;
    struct arr_node * prev;
} arr_node_t;

typedef struct idx_node {
    uint32_t          x_idx;
    uint32_t          y_idx;
    struct idx_node * next;
} idx_node_t;

typedef struct arr_list {
    arr_node_t **     arr;
    arr_node_t *      ll;
    idx_node_t *      free_idx_que;
    uint32_t          log_init_size;
    volatile uint32_t nitems;
    pthread_mutex_t   m;
} arr_list_t;

#define MAX_ARRAY 32

arr_list_t * init_alist(uint32_t total_items);
void         free_alist(arr_list_t * a_list);

void         remove_node_idx(arr_list_t * alist, uint32_t idx);
void         remove_node_lock(arr_list_t * alist, arr_node_t * drop_node);
void         remove_node(arr_list_t * alist, arr_node_t * drop_node);
arr_node_t * add_node(arr_list_t * alist, void * usr_key, void * usr_data);

arr_node_t * get_node_idx(arr_list_t * alist, uint32_t idx);

void print_nodes(arr_list_t * alist, void print_usr_data(arr_node_t *));
#endif
