#include <datastructs/arr_list.h>


arr_list_t *
init_alist(uint32_t init_size) {
    const uint32_t _init_size =
        init_size ? roundup_32(init_size) : DEFAULT_INIT_SIZE;

    arr_list_t * new_alist = (arr_list_t *)mycalloc(1, sizeof(arr_list_t));

    new_alist->arr = (arr_node_t **)mycalloc(MAX_ARRAY, sizeof(arr_node_t *));
    new_alist->arr[0] = (arr_node_t *)mycalloc(_init_size, sizeof(arr_node_t));

    new_alist->log_init_size = ulog2_32(_init_size);

    pthread_mutex_init(&(new_alist->m), NULL);
    return new_alist;
}


void
free_alist(arr_list_t * alist) {
    for (uint32_t i = 0; i < MAX_ARRAY; i++) {
        myfree(alist->arr[i]);
    }
    myfree(alist->arr);
    myfree(alist);
}

static void
add_to_que(arr_list_t * alist, idx_node_t * new_idx_node) {
    new_idx_node->next  = alist->free_idx_que;
    alist->free_idx_que = new_idx_node;
}

static idx_node_t *
check_pop_que(arr_list_t * alist) {
    if (alist->free_idx_que) {
        idx_node_t * ret    = alist->free_idx_que;
        alist->free_idx_que = alist->free_idx_que->next;
        return ret;
    }
    return NULL;
}

static void
add_to_list(arr_list_t * alist, arr_node_t * new_node) {
    if (alist->ll == NULL) {
        alist->ll = new_node;
    }
    else {
        new_node->next  = alist->ll;
        alist->ll->prev = new_node;
        alist->ll       = new_node;
    }
}


static void
remove_from_list(arr_list_t * alist, arr_node_t * drop_node) {
    if (drop_node->next == NULL && drop_node->prev == NULL) {
        alist->ll = NULL;
    }
    else if (drop_node->next != NULL && drop_node->prev == NULL) {
        alist->ll       = drop_node->next;
        alist->ll->prev = NULL;
    }
    else if (drop_node->next == NULL && drop_node->prev != NULL) {
        drop_node->prev->next = NULL;
    }
    else {
        drop_node->prev->next = drop_node->next;
        drop_node->next->prev = drop_node->prev;
    }
}


static uint64_t
get_x_y_idx(uint32_t idx, uint32_t log_init_size) {

    uint32_t x_idx = IDX_TO_X(idx, log_init_size);
    uint32_t y_idx = IDX_TO_Y(idx, x_idx, log_init_size);

    DBG_ASSERT(x_idx < MAX_ARRAY,
               "Error: invalid x index (%d >= %d)\n",
               x_idx,
               MAX_ARRAY);
    DBG_ASSERT(y_idx < X_TO_SIZE(x_idx, log_init_size),
               "Error: invalid y index (%d >= %d)\n",
               y_idx,
               X_TO_SIZE(x_idx, log_init_size));

    DBG_ASSERT(idx == x_y_to_idx(x_idx, y_idx, log_init_size),
               "Error x_y_to_idx does not match idx\n\t"
               "x_idx   : %d\n\t"
               "y_idx   : %d\n\t"
               "idx     : %d != %d\n",
               x_idx,
               y_idx,
               idx,
               x_y_to_idx(x_idx, y_idx, log_init_size));


    uint64_t ret = y_idx;
    ret <<= sizeof_bits(uint32_t);
    ret |= x_idx;
    return ret;
}


arr_node_t *
add_node(arr_list_t * alist, void * usr_key, void * usr_data) {
    arr_node_t * new_node = NULL;
    pthread_mutex_lock(&(alist->m));
    idx_node_t * check_idx = check_pop_que(alist);
    pthread_mutex_unlock(&(alist->m));
    if (check_idx) {
        new_node = (arr_node_t *)check_idx;
    }
    else {
        uint32_t log_init_size = alist->log_init_size;
        uint32_t idx =
            __atomic_fetch_add(&(alist->nitems), 1, __ATOMIC_RELAXED);
        uint64_t x_y_idx = get_x_y_idx(idx, log_init_size);
        uint32_t x_idx   = get_x_idx(x_y_idx);
        uint32_t y_idx   = get_y_idx(x_y_idx);

        if (alist->arr[x_idx] == NULL) {
            pthread_mutex_lock(&(alist->m));
            if (alist->arr[x_idx] == NULL) {
                alist->arr[x_idx] =
                    (arr_node_t *)mycalloc(X_TO_SIZE(x_idx, log_init_size),
                                           sizeof(arr_node_t));
            }
            pthread_mutex_unlock(&(alist->m));
        }
        new_node = alist->arr[x_idx] + y_idx;

        DBG_ASSERT(new_node->usr_data == NULL && new_node->x_idx == 0 &&
                       new_node->y_idx == 0,
                   "Error new node is not new....\n");

        new_node->x_idx = x_idx;
        new_node->y_idx = y_idx;
    }

    new_node->usr_key  = usr_key;
    new_node->usr_data = usr_data;

    pthread_mutex_lock(&(alist->m));
    add_to_list(alist, new_node);
    pthread_mutex_unlock(&(alist->m));
    return new_node;
}

arr_node_t *
get_node_idx(arr_list_t * alist, uint32_t idx) {
    uint32_t log_init_size = alist->log_init_size;
    uint64_t x_y_idx       = get_x_y_idx(idx, log_init_size);


    uint32_t x_idx = get_x_idx(x_y_idx);
    uint32_t y_idx = get_y_idx(x_y_idx);

    if (idx >= alist->nitems || alist->arr[x_idx] == NULL ||
        y_idx >= X_TO_SIZE(x_idx, log_init_size)) {
        return NULL;
    }

    arr_node_t * ret_node = alist->arr[x_idx] + y_idx;

#ifdef SUPER_DEBUG
    DBG_ASSERT(ret_node != NULL && ret_node->x_idx == x_idx &&
                   ret_node->y_idx == y_idx,
               "Error invalid index\n\t"
               "x_idx   : %d vs %d\n\t"
               "y_idx   : %d vs %d\n\t"
               "idx     : %d\n",
               x_idx,
               ret_node->x_idx,
               y_idx,
               ret_node->y_idx,
               idx);
#endif
    return ret_node;
}

void
remove_node_idx(arr_list_t * alist, uint32_t idx) {
    arr_node_t * drop_node = get_node_idx(alist, idx);
    pthread_mutex_lock(&(alist->m));
    remove_node(alist, drop_node);
    pthread_mutex_unlock(&(alist->m));
}

void
remove_node_lock(arr_list_t * alist, arr_node_t * drop_node) {
    pthread_mutex_lock(&(alist->m));
    remove_node(alist, drop_node);
    pthread_mutex_unlock(&(alist->m));
}

void
remove_node(arr_list_t * alist, arr_node_t * drop_node) {
    remove_from_list(alist, drop_node);
    add_to_que(alist, (idx_node_t *)drop_node);
}


void
print_nodes(arr_list_t * alist, void print_usr_data(arr_node_t *)) {
    pthread_mutex_lock(&(alist->m));
    arr_node_t * temp = alist->ll;
    while (temp) {
        uint32_t idx =
            x_y_to_idx(temp->x_idx, temp->y_idx, alist->log_init_size);

        fprintf(stderr,
                PRINT_FORMAT_STR_PROLOGUE,
                idx,
                MAX_IDX_LEN - (int32_t)log10((double)(idx + (idx == 0))),
                ' ');

        print_usr_data(temp);
        fprintf(stderr, PRINT_FORMAT_STR_EPILOGUE, EPI_PRINT_OFFSET, ' ');
        temp = temp->next;
    }
    pthread_mutex_unlock(&(alist->m));
}
