#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_


//////////////////////////////////////////////////////////////////////
// Configs
#include <general-config.h>

#define cache_line_size L1_CACHE_LINE_SIZE
#define log_cls         L1_LOG_CACHE_LINE_SIZE

#define ent_per_line cache_line_size / sizeof(node *)
// this is step length for iterating through
// int array where each index is on its own cache line
#define int_ca     (cache_line_size >> 2)
#define log_int_ca (log_cls - 2)

#define num_high_bits 16

//#define mark_null //table being copied will not add to null items (if they can
// mark it) #define next_hash //turns on storing next hash optimization #define
// lazy_resize //turns on lazy resizing (necessary for functioning delete)
#if defined next_hash && !defined lazy_resize
    #undef next_hash
#endif
#if defined mark_null && !defined lazy_resize
    #undef mark_null
#endif


#define DEFAULT_INIT_TSIZE         (sizeof_bits(cpu_set_t))
#define DEFAULT_SPREAD             (16)
#define DEFAULT_HASH_ATTEMPTS      (1)
#define DEFAULT_LINES              (1)
#define DEFAULT_READS_PER_LINE     (ent_per_line * DEFAULT_LINES)
#define DEFAULT_LOG_READS_PER_LINE (3)


#define counter_bits      4
#define counter_bits_mask 0xf
#define slot_bits         12
#define slot_bits_mask    0xfff

#define RESIZE_THRESHOLD 2
#define max_tables       64  // max tables to create
//////////////////////////////////////////////////////////////////////


#include <helpers/bits.h>
#include <helpers/opt.h>
#include <helpers/util.h>


//////////////////////////////////////////////////////////////////////
// arbitrary constants
#define EQUALS     1
#define NOT_EQUALS 0


// return values for checking table.  Returned by lookupQuery
#define not_in     (-3)  // item is not in (used in query/delete)
#define is_in      (-1)  // item is already in
#define unknown_in (-2)  // unkown keep looking

#define copy_bit 0x1  // sets 1s bit of ent ptr for resizing


#define getCopy(X) lowBitsGet((void *)(X))
#define setCopy(X)                                                             \
    lowBitsSet_atomic((void **)(&(X)), lowBitsGetPtr((void *)(X)), copy_bit)


#define set_return(X, Y) ((node *)(((uint64_t)get_ptr(X)) | (Y)))

#define getNodePtr(X) (getPtr((void *)(X)))

#define getCounter(X)    (highBitsGet((void *)(X)) & (counter_bits_mask))
#define decrCounter(X)   (highBitsSetDECR((void **)(&(X))))
#define incrCounter(X)   (highBitsSetINCR((void **)(&(X))))
#define subCounter(X, Y) (highBitsSetSUB((void **)(&(X)), (Y)))
#define setCounter(X, Y)                                                       \
    (highBitsSetMASK((void **)(&(X)), (Y), counter_bits_mask))

#define getNH(X, Y) ((highBitsGet((void *)(X))) >> (counter_bits + (Y)))
#define setNH(X, Y) (highBitsSet((void **)(&(X)), (Y)))


// table config is necessary
//////////////////////////////////////////////////////////////////////
// config for type hashtable will use

typedef struct frame_node {
    pthread_t key;
    void *    val;  // this is a frame_data_t ** defined in debug
} frame_node;

// define node and key type
typedef pthread_t         key_type;
typedef struct frame_node node;


uint16_t genTag(pthread_t t);

// nothing for ints
#define hashTag(X) (genTag(X))

// compare key comparison
#define compare_keys(X, Y) ((X) == (Y))

#define getKey(X)    (((node *)getNodePtr(X))->key)
#define getVal(X)    (((node *)getNodePtr(X))->val)
#define getKeyLen(X) sizeof(pthread_t)

// hash function for int (key is passed as a ptr)
#define hashFun(X, Y)                                                          \
    murmur3_32((const uint8_t *)(&(X)), sizeof(pthread_t), (Y));
#define getKeyTag(X) genTag(X)


//////////////////////////////////////////////////////////////////////
// Config for hash function table will use
uint32_t murmur3_32(const uint8_t * key, size_t len, uint32_t seed);
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// table structs
// a sub table (this should be hidden)
typedef struct subTable {
    node ** innerTable;  // rows (table itself)

    // for keeping track of when all items from min sub table have been moved
#ifdef lazy_resize
    uint32_t * threadCopy;
#endif
    uint32_t tableSize;  // size
    uint32_t logTableSize;
} subTable;


// head of cache: this is the main hahstable
typedef struct hashTable {
    subTable **    tableArray;  // array of tables
    uint64_t       start;
    uint64_t       end;  // current max index (max exclusive)
    const uint32_t seeds[DEFAULT_HASH_ATTEMPTS];
} hashTable;  // worth it to aligned alloc this


//////////////////////////////////////////////////////////////////////
#ifdef cache
    #define incr(X, Y, Z) (X)[((Y) << log_int_ca)] += (Z)
    #define decr(X, Y, Z) (X)[((Y) << log_int_ca)] -= (Z)
#else
    #define incr(X, Y, Z) (X)[(Y)] += (Z)
    #define decr(X, Y, Z) (X)[(Y)] -= (Z)
#endif  // cache


//////////////////////////////////////////////////////////////////////
// general api start
// return 1 if inserted, 0 if already there

node * addNode(hashTable * head, node * n, uint32_t tid);

// see if node is in the table
node * findNode(hashTable * table, key_type key, uint32_t tid);


// initialize a new main hashtable
hashTable * initTable();


void freeTable(hashTable * head, void (*freeEnt)(void *));

//////////////////////////////////////////////////////////////////////
// general api end

#endif
