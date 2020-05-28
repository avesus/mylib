#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#include <helpers/bits.h>
#include <helpers/opt.h>
#include <helpers/util.h>

//////////////////////////////////////////////////////////////////////
// Configs
#include <general-config.h>


//#define PIQ_MARK_NULL //table being copied will not add to null items (if they
//can
// mark it) #define next_hash //turns on storing next hash optimization #define
// lazy_resize //turns on lazy resizing (necessary for functioning delete)
#if defined PIQ_NEXT_HASH && !defined PIQ_LAZY_RESIZE
#undef PIQ_NEXT_HASH
#endif
#if defined PIQ_MARK_NULL && !defined PIQ_LAZY_RESIZE
#undef PIQ_MARK_NULL
#endif


#define PIQ_DEFAULT_INIT_TSIZE    (sizeof_bits(cpu_set_t))
#define PIQ_DEFAULT_SPREAD        (16)
#define PIQ_DEFAULT_HASH_ATTEMPTS (1)
#define PIQ_DEFAULT_LINES         (1)
#define PIQ_DEFAULT_READS_PER_LINE                                             \
    ((L1_CACHE_LINE_SIZE / sizeof(void *)) * PIQ_DEFAULT_LINES)
#define PIQ_DEFAULT_LOG_READS_PER_LINE (3)
#define PIQ_RESIZE_THRESHOLD           2


//////////////////////////////////////////////////////////////////////
// actual node type
typedef struct frame_node {
    pthread_t key;
    void *    val;  // this is a frame_data_t ** defined in debug
} frame_node_t;

// define node and key type
typedef pthread_t         piq_key_t;
typedef struct frame_node piq_node_t;


//////////////////////////////////////////////////////////////////////
// table structs
// a sub table (this should be hidden)
typedef struct piq_subtable {
    piq_node_t ** innerTable;  // rows (table itself)

    // for keeping track of when all items from min sub table have been moved
#ifdef lazy_resize
    uint32_t * threadCopy;
#endif
    uint32_t tableSize;  // size
    uint32_t logTableSize;
} piq_subtable_t;

// head of cache: this is the main hahstable
typedef struct piq_ht {
    piq_subtable_t ** tableArray;  // array of tables
    uint64_t          start;
    uint64_t          end;  // current max index (max exclusive)
    const uint32_t    seeds[PIQ_DEFAULT_HASH_ATTEMPTS];
} piq_ht;  // worth it to aligned alloc this


//////////////////////////////////////////////////////////////////////
// general api start
// return 1 if inserted, 0 if already there

piq_node_t * piq_add_node(piq_ht * head, piq_node_t * n, uint32_t tid);

// see if piq_node_t is in the table
piq_node_t * piq_find_node(piq_ht * table, piq_key_t key, uint32_t tid);


// initialize a new main hashtable
piq_ht * piq_init_table();


void piq_free_table(piq_ht * head, void (*free_ent)(void *));

//////////////////////////////////////////////////////////////////////
// general api end


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// undefs


#endif
