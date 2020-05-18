#include <datastruct/arrlist.h>

#define REMOVED_IDX (~((0u)))

#define GET_MEM_PTR(X)   ((X)->mem_addr

#define CHECK_REMOVED(X) ((X)->prev == REMOVED_IDX)
#define SET_REMOVED(X)   ((X)->prev = REMOVED_IDX)

#define TO_INT(X, Y) ((uint32_t)((uint64_t)(X) - (Y)))

arr_list_t *
init_alist() {

    const uint64_t _max_size =
        sizeof(arr_list_t) + (DEFAULT_INIT_AL_SIZE * sizeof(arr_node_t));

    uint64_t start_addr =
        (uint64_t)mymmap(0,
                         _max_size,
                         PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE,
                         (-1),
                         0);


    arr_list_t * new_alist = (arr_list_t *)start_addr;
    new_alist->mem_addr    = start_addr;

    return new_alist;
}


void
free_alist(arr_list_t * alist) {
    const uint64_t _max_size =
        sizeof(arr_list_t) + (DEFAULT_INIT_AL_SIZE * sizeof(arr_node_t));
    mymunmap(alist, _max_size);
}

static void
add_to_que(arr_list_t * alist, arr_node_t * new_idx_node) {
    new_idx_node->next  = alist->free_idx_que;
    alist->free_idx_que = TO_INT(new_idx_node, alist->mem_addr);
}

static arr_node_t *
check_pop_que(arr_list_t * alist) {
    if (alist->free_idx_que) {
        arr_node_t * ret =
            (arr_node_t *)AL_TO_PTR(alist->free_idx_que, alist->mem_addr);
        alist->free_idx_que =
            AL_TO_PTR(alist->free_idx_que, alist->mem_addr)->next;
        return ret;
    }
    return NULL;
}

static void
add_to_list(arr_list_t * alist, arr_node_t * new_node) {
    new_node->prev = 0;
    if (alist->ll == 0) {
        alist->ll      = TO_INT(new_node, alist->mem_addr);
        new_node->next = 0;
    }
    else {
        new_node->next = alist->ll;
        AL_TO_PTR(alist->ll, alist->mem_addr)->prev =
            TO_INT(new_node, alist->mem_addr);
        alist->ll = TO_INT(new_node, alist->mem_addr);
    }
}


static void
remove_from_list(arr_list_t * alist, arr_node_t * drop_node) {
    if (drop_node->next == 0 && drop_node->prev == 0) {
        alist->ll = 0;
    }
    else if (drop_node->next != 0 && drop_node->prev == 0) {
        alist->ll                                   = drop_node->next;
        AL_TO_PTR(alist->ll, alist->mem_addr)->prev = 0;
    }
    else if (drop_node->next == 0 && drop_node->prev != 0) {
        AL_TO_PTR(drop_node->prev, alist->mem_addr)->next = 0;
    }
    else {
        AL_TO_PTR(drop_node->prev, alist->mem_addr)->next = drop_node->next;
        AL_TO_PTR(drop_node->next, alist->mem_addr)->prev = drop_node->prev;
    }
}


arr_node_t *
add_node(arr_list_t * alist, void * data) {
    arr_node_t * new_node  = NULL;
    arr_node_t * check_idx = check_pop_que(alist);
    if (check_idx) {
        new_node = check_idx;
    }
    else {
        const uint32_t idx = alist->nitems++;
        new_node = (arr_node_t *)(alist->mem_addr + sizeof(arr_list_t) +
                                  idx * sizeof(arr_node_t));
    }

    new_node->data = data;

    add_to_list(alist, new_node);
    assert(new_node->data == data);
    return new_node;
}

arr_node_t *
get_node_idx(arr_list_t * alist, uint32_t idx) {
    if (idx >= alist->nitems) {
        return NULL;
    }

    arr_node_t * ret_node =
        (arr_node_t *)(alist->mem_addr + sizeof(arr_list_t) +
                       idx * sizeof(arr_node_t));

    return CHECK_REMOVED(ret_node) ? NULL : ret_node;
}

void
remove_node_idx(arr_list_t * alist, uint32_t idx) {
    arr_node_t * drop_node = get_node_idx(alist, idx);
    remove_node(alist, drop_node);
}


void
remove_node(arr_list_t * alist, arr_node_t * drop_node) {
    remove_from_list(alist, drop_node);
    SET_REMOVED(drop_node);
    add_to_que(alist, drop_node);
}

arr_node_t *
pop_node(arr_list_t * alist) {
    if (alist->ll) {
        arr_node_t * ret_node = AL_TO_PTR(alist->ll, alist->mem_addr);
        remove_from_list(alist, ret_node);
        return ret_node;
    }
    return NULL;
}


uint32_t
count_que(arr_list_t * alist) {
    uint32_t temp  = alist->free_idx_que;
    uint32_t count = 0;
    while (temp) {
        count++;
        temp = AL_TO_PTR(temp, alist->mem_addr)->next;
    }
    return count;
}

uint32_t
count_ll(arr_list_t * alist) {
    uint32_t temp  = alist->ll;
    uint32_t count = 0;
    while (temp) {
        count++;
        temp = AL_TO_PTR(temp, alist->mem_addr)->next;
    }
    return count;
}
