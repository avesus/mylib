#include <local/hashtable.h>


//////////////////////////////////////////////////////////////////////
// Defines that should not be shared with other files


// return values for checking table.  Returned by lookupQuery
#define not_in     (-3)  // item is not in (used in query/delete)
#define is_in      (-1)  // item is already in
#define unknown_in (-2)  // unkown keep looking

#define copy_bit 0x1  // sets 1s bit of ent ptr for resizing


#define counter_bits      4
#define counter_bits_mask 0xf
#define slot_bits         12
#define slot_bits_mask    0xfff
#define max_tables        64  // max tables to create


#define getCopy(X) lowBitsGet((void *)(X))
#define setCopy(X)                                                             \
    v lowBitsSet_atomic((void **)(&(X)), lowBitsGetPtr((void *)(X)), copy_bit)


#define set_return(X, Y) ((piq_node_t *)(((uint64_t)get_ptr(X)) | (Y)))

#define getNodePtr(X) (getPtr((void *)(X)))

#define getCounter(X)    (highBitsGet((void *)(X)) & (counter_bits_mask))
#define decrCounter(X)   (highBitsSetDECR((void **)(&(X))))
#define incrCounter(X)   (highBitsSetINCR((void **)(&(X))))
#define subCounter(X, Y) (highBitsSetSUB((void **)(&(X)), (Y)))
#define setCounter(X, Y)                                                       \
    (highBitsSetMASK((void **)(&(X)), (Y), counter_bits_mask))

#define getNH(X, Y) ((highBitsGet((void *)(X))) >> (counter_bits + (Y)))
#define setNH(X, Y) (highBitsSet((void **)(&(X)), (Y)))


#ifdef cache
#define incr(X, Y, Z) (X)[((Y) << log_int_ca)] += (Z)
#define decr(X, Y, Z) (X)[((Y) << log_int_ca)] -= (Z)
#else
#define incr(X, Y, Z) (X)[(Y)] += (Z)
#define decr(X, Y, Z) (X)[(Y)] -= (Z)
#endif  // cache
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// config for type hashtable will use

uint16_t genTag(pthread_t t);

// nothing for ints
#define hashTag(X) (genTag(X))

// compare key comparison
#define compare_keys(X, Y) ((X) == (Y))

#define getKey(X)    (((piq_node_t *)getNodePtr(X))->key)
#define getVal(X)    (((piq_node_t *)getNodePtr(X))->val)
#define getKeyLen(X) sizeof(pthread_t)

// hash function for int (key is passed as a ptr)
#define hashFun(X, Y)                                                          \
    murmur3_32((const uint8_t *)(&(X)), sizeof(pthread_t), (Y));
#define getKeyTag(X) genTag(X)

//////////////////////////////////////////////////////////////////////
// Config for hash function table will use
static uint32_t murmur3_32(const uint8_t * key, size_t len, uint32_t seed);
//////////////////////////////////////////////////////////////////////


#ifdef PIQ_LAZY_RESIZE
static uint32_t add_node_resize(piq_ht *     head,
                                uint32_t     start,
                                piq_node_t * n,
                                uint32_t     tid,
                                uint16_t     tag
#ifdef PIQ_NEXT_HASH
                                ,
                                uint32_t from_slot
#endif  // PIQ_NEXT_HASH
);
static void resize_node(piq_ht *         head,
                        piq_subtable_t * ht,
                        uint32_t         slot,
                        uint32_t         tid);
#endif  // PIQ_LAZY_RESIZE


//////////////////////////////////////////////////////////////////////
// hash function defined here
static uint32_t
murmur3_32(const uint8_t * key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    if (len > 3) {
        const uint32_t * key_x4 = (const uint32_t *)key;
        size_t           i      = len >> 2;
        do {

            uint32_t k = *key_x4++;
            k *= 0xcc9e2d51;
            k = (k << 15) | (k >> 17);
            k *= 0x1b873593;
            h ^= k;
            h = (h << 13) | (h >> 19);
            h = h * 5 + 0xe6546b64;
        } while (--i);
        key = (const uint8_t *)key_x4;
    }
    if (len & 3) {
        size_t   i = len & 3;
        uint32_t k = 0;
        key        = &key[i - 1];
        do {
            k <<= 8;
            k |= *key--;
        } while (--i);
        k *= 0xcc9e2d51;
        k = (k << 15) | (k >> 17);
        k *= 0x1b873593;
        h ^= k;
    }
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}


uint16_t
genTag(piq_key_t t) {
    t ^= t >> 32;
    t ^= t >> 16;
    return t & 0xffff;
}

#ifdef PIQ_LAZY_RESIZE
// helper function that returns sum of array up to size
static uint32_t
sumArr(uint32_t * arr, uint32_t size) {
    uint32_t sum = 0;
    size <<= log_int_ca;
    for (uint32_t i = 0; i < size; i += int_ca) {

        sum += arr[i];
    }

    return sum;
}
#endif

#ifdef PIQ_NEXT_HASH
// creates short to store in high bits of piq_node_t * with info on next slot
// locations
static uint16_t
createNHMask(uint32_t hv, uint32_t ltsize) {
    hv >>= ltsize;
    uint32_t nbits = MIN(32 - ltsize, slot_bits);
    hv &= ((1 << nbits) - 1);
    // idea here is that if less than 12 bits need to have them start at
    // offset and change so that can use lower counter value (because
    // which bit to use is calculated inversely)
    hv <<= (slot_bits - nbits);
    uint16_t hb = nbits;
    hb |= (hv << counter_bits);
    return hb;
}
#endif

// creates a subtable
static piq_subtable_t *
createST(piq_ht * head, uint32_t tsize) {
    piq_subtable_t * ht = (piq_subtable_t *)myacalloc(L1_CACHE_LINE_SIZE,
                                                      1,
                                                      sizeof(piq_subtable_t));

    // calloc better for large allocations and not important for this one to be
    // aligned
    ht->innerTable = (piq_node_t **)mycalloc(tsize, sizeof(piq_node_t *));

#ifdef PIQ_LAZY_RESIZE
    ht->threadCopy =
        (uint32_t *)mycalloc(PIQ_DEFAULT_SPREAD, L1_CACHE_LINE_SIZE);
#endif  // PIQ_LAZY_RESIZE

    ht->logTableSize = ulog2_32(tsize);
    ht->tableSize    = tsize;
    return ht;
}

// frees a given subtable that was created for adddrop (that failed)
static void
freeST(piq_subtable_t * table) {
    myfree(table->innerTable);
#ifdef PIQ_LAZY_RESIZE
    myfree(table->threadCopy);
#endif  // PIQ_LAZY_RESIZE
    myfree(table);
}


// returns whether an node is found in a given subtable/overall hashtable. notIn
// means not in the hashtable, s means in the hashtable and was found (return s
// so can get the value of the item). unk means unknown if in table.
static uint32_t
lookupQuery(piq_subtable_t * ht, piq_key_t key, uint32_t slot) {
    // if find null slot know item is not in hashtable as would have been added
    // there otherwise
    if (getNodePtr(ht->innerTable[slot]) == NULL) {
#ifdef PIQ_MARK_NULL
        if (getCopy(ht->innerTable[slot])) {
            return unknown;
        }
#endif  // PIQ_MARK_NULL
        return not_in;
    }

    // values equal return index so can access later
    else if (compare_keys(getKey(ht->innerTable[slot]), key)) {
        return slot;
    }

    // go another value, try next hash function
    return unknown_in;
}


// check if node for a given hashing vector is in a table. Returns in if node is
// already in the table, s if the value is not in the table, and unk to try the
// next hash function
static uint32_t
lookup(piq_ht *         head,
       piq_subtable_t * ht,
       piq_node_t *     n,
       uint32_t         slot,
       uint32_t         tid
#ifdef PIQ_LAZY_RESIZE
       ,
       uint32_t doCopy,
       uint32_t start
#endif  // PIQ_LAZY_RESIZE //
) {

    // if found null slot return index so insert can try and put the piq_node_t
    // in the index
    if (getNodePtr(ht->innerTable[slot]) == NULL) {
#if defined PIQ_MARK_NULL && defined PIQ_LAZY_RESIZE
        if (doCopy) {
            if (setCopy(ht->innerTable[slot])) {
                resize_node(head, ht, slot, tid);
            }
        }
#endif  // PIQ_MARK_NULL && PIQ_LAZY_RESIZE
        return slot;
    }

    // if values equal case
    else if (compare_keys(getKey(ht->innerTable[slot]), getKey(n))) {

        return is_in;
    }

#ifdef PIQ_LAZY_RESIZE
    // neither know if value is in or not, first check if this is smallest
    // subtable and resizing is take place. If so move current item at subtable
    // to next one.
    if (doCopy) {
        if (setCopy(ht->innerTable[slot])) {
            // succesfully set by this thread
            resize_node(head, ht, slot, tid);
        }
    }
#endif  // PIQ_LAZY_RESIZE
    return unknown_in;
}

// function to add new subtable to hashtable if dont find open slot
static void
addDrop(piq_ht *         head,
        piq_subtable_t * toadd,
        uint64_t         addSlot,
        piq_node_t *     n,
        uint32_t         tid,
        uint32_t         start) {

    // make the array circular so add/delete continuous will always have space.
    // Assumed that resizer will keep up with insertion rate (this is probably
    // provable as when resize is active (which occurs when num subtables >
    // threshhold each insert brings an old item with it). Also if the diff
    // between max/min subtables exceed a certain bound will start doubling
    // again irrelivant of delete/insert ratio

    // try and add new preallocated table (CAS so only one added)
    piq_subtable_t * expected = NULL;
    uint32_t         res = __atomic_compare_exchange(&head->tableArray[addSlot],
                                             &expected,
                                             &toadd,
                                             1,
                                             __ATOMIC_RELAXED,
                                             __ATOMIC_RELAXED);
    if (res) {
        // if succeeded try and update new max then insert item
        uint64_t newSize = addSlot + 1;
        __atomic_compare_exchange(&head->end,
                                  &addSlot,
                                  &newSize,
                                  1,
                                  __ATOMIC_RELAXED,
                                  __ATOMIC_RELAXED);
    }
    else {
        // if failed free subtable then try and update new max then insert item
        freeST(toadd);
        uint64_t newSize = addSlot + 1;
        __atomic_compare_exchange(&head->end,
                                  &addSlot,
                                  &newSize,
                                  1,
                                  __ATOMIC_RELAXED,
                                  __ATOMIC_RELAXED);
    }
}


#ifdef PIQ_LAZY_RESIZE
// wrapper for resizing node, will call add resize if necessary and
// possible increment head's table index
static void
resize_node(piq_ht * head, piq_subtable_t * ht, uint32_t slot, uint32_t tid) {

    // exstart/newstart for CAS that might take place
    uint64_t exStart  = head->start;
    uint64_t newStart = exStart + 1;

// if item is not deleted copy up (this is how deleted items are culled as
// they will not be copied)
#ifdef PIQ_MARK_NULL
    if (getNodePtr(ht->innerTable[slot])) {
#endif  // PIQ_MARK_NULL

        add_node_resize(head,
                        head->start + 1,
                        lowBitsGetPtr(ht->innerTable[slot]),
                        tid,
                        getKeyTag(getKey(ht->innerTable[slot]))
#ifdef PIQ_NEXT_HASH
                            ,
                        slot
#endif  // PIQ_NEXT_HASH
        );

#ifdef PIQ_MARK_NULL
    }
#endif  // PIQ_MARK_NULL
    // increment thread index
    incr(ht->threadCopy, tid, 1);
    // if all slots have been copied increment min subtable
    if (ht->tableSize == sumArr(ht->threadCopy, PIQ_DEFAULT_SPREAD)) {
        if (__atomic_compare_exchange(&head->start,
                                      &exStart,
                                      &newStart,
                                      1,
                                      __ATOMIC_RELAXED,
                                      __ATOMIC_RELAXED)) {
        }
    }
}

// addnode resize we have different set of conditions that we can
// optimize around i.e should never find self deleted, dont need to
// recompute tag, and later will remove need for rehashing
static uint32_t
add_node_resize(piq_ht *     head,
                uint32_t     start,
                piq_node_t * n,
                uint32_t     tid,
                uint16_t     tag
#ifdef PIQ_NEXT_HASH
                ,
                uint32_t from_slot
#endif  // PIQ_NEXT_HASH
) {


    const uint32_t logReadsPerLine = PIQ_DEFAULT_LOG_READS_PER_LINE;
    const uint32_t uBound          = PIQ_DEFAULT_READS_PER_LINE;
    const uint32_t ha              = PIQ_DEFAULT_HASH_ATTEMPTS;

    uint32_t         localEnd = head->end;
    piq_subtable_t * ht       = NULL;


//////////////////////////////////////////////////////////////////////
// next hash optimization start
#ifdef PIQ_NEXT_HASH
    from_slot >>= logReadsPerLine;
    from_slot <<= logReadsPerLine;

    uint32_t amount_next_bits = getCounter(n);
    uint32_t hb_index         = slot_bits - amount_next_bits;
    amount_next_bits += start;
    piq_subtable_t * prev = NULL;
    ht                    = head->tableArray[(start - 1) & (max_tables - 1)];
    for (uint32_t j = start; j < amount_next_bits; j++) {
        // prev for checking if new slot bit is needed
        prev = ht;
        ht   = head->tableArray[j];

        // tables wont grow in size if delete ratio is high enough this
        // means just reused old slot
        if (ht->tableSize != prev->tableSize) {
            //	fprintf(stderr, "%d:: %x |= ((%x>>%d (%x) )&0x1) << %d -> ", j,
            // from_slot, getNH(n, 0), hb_index, getNH(n, hb_index),
            //(ht->logTableSize-1));
            from_slot |= (getNH(n, hb_index) & 0x1) << (ht->logTableSize - 1);
            hb_index++;
        }

        // iterate through hash functions
        for (uint32_t i = 0; i < ha; i++) {
            uint32_t slot = from_slot;
            for (uint32_t c = tag; c < uBound + tag; c++) {
                uint32_t res = lookup(head,
                                      ht,
                                      n,
                                      slot + (c & (uBound - 1)),
                                      tid
#ifdef PIQ_LAZY_RESIZE
                                      ,
                                      0,
                                      j
#endif  // PIQ_LAZY_RESIZE
                );

                // lookup value in sub table
                if (res == unknown) {  // unkown if in our not
                    continue;
                }
                else if (res == in) {  // is in
                    return 0;
                }
                else {

                    // if return was null (is available slot in sub table) try
                    // and add with CAS. if succeed return 1, else if value it
                    // lost to is item itself return. If neither continue trying
                    // to add

                    // update counter based on bits used
                    setCounter(n, slot_bits - hb_index);
                    piq_node_t * expected = NULL;
                    uint32_t     cmp =
                        __atomic_compare_exchange((ht->innerTable + res),
                                                  &expected,
                                                  &n,
                                                  1,
                                                  __ATOMIC_RELAXED,
                                                  __ATOMIC_RELAXED);
                    if (cmp) {
                        // if we win CAS increment insert count and return 1

                        return 1;
                    }
                    else {
                        // else check if value that was lost to is same, if not
                        // keep going, if is turn 0
                        if (compare_keys(getKey(ht->innerTable[res]),
                                         getKey(n))) {
                            return 0;
                        }
                    }
                }
            }
        }

        // update locacur

        localEnd = head->end;
        // could just drop through here but imo
        // should keep trying to reused bits
        if (j >= (head->end - 1)) {

            // if found no available slot in hashtable create new subtable and
            // add it to hashtable then try insertion again

            // next table size defaults to same size
            uint32_t nextTableSize =
                head->tableArray[(localEnd - 1) & (max_tables - 1)]->tableSize
                << 1;

            // create next subtables
            piq_subtable_t * new_table = createST(head, nextTableSize);
            addDrop(head, new_table, localEnd, n, tid, start + 1);
        }
    }

#endif  // PIQ_NEXT_HASH
    //////////////////////////////////////////////////////////////////////
    // next hash optimization end


    uint32_t buckets[PIQ_DEFAULT_HASH_ATTEMPTS];
    for (uint32_t i = 0; i < ha; i++) {
        buckets[i] = hashFun(getKey(n), head->seeds[i]);
    }
    while (1) {
        // iterate through subtables
        // again is mod max_subtables
        for (uint32_t j = start; j < head->end; j++) {
            ht = head->tableArray[j];

            uint32_t tsizeMask = ((ht->tableSize - 1) >> logReadsPerLine)
                                 << logReadsPerLine;


            // iterate through hash functions
            for (uint32_t i = 0; i < ha; i++) {

                uint32_t slot = (buckets[i] & tsizeMask);
                for (uint32_t c = tag; c < uBound + tag; c++) {
                    uint32_t res = lookup(head,
                                          ht,
                                          n,
                                          slot + (c & (uBound - 1)),
                                          tid
#ifdef PIQ_LAZY_RESIZE
                                          ,
                                          0,
                                          j
#endif  // PIQ_LAZY_RESIZE
                    );


                    // lookup value in sub table

                    if (res == unknown) {  // unkown if in our not
                        continue;
                    }
                    else if (res == in) {  // is in
                        return 0;
                    }

                    else {

                        // if return was null (is available slot in sub table)
                        // try and add with CAS. if succeed return 1, else if
                        // value it lost to is item itself return. If neither
                        // continue trying to add

#ifdef PIQ_NEXT_HASH
                        uint16_t next_bits =
                            createNHMask(buckets[0], ht->logTableSize);
                        setNH(n, next_bits);
#endif  // PIQ_NEXT_HASH

                        piq_node_t * expected = NULL;
                        uint32_t     cmp =
                            __atomic_compare_exchange((ht->innerTable + res),
                                                      &expected,
                                                      &n,
                                                      1,
                                                      __ATOMIC_RELAXED,
                                                      __ATOMIC_RELAXED);
                        if (cmp) {
                            // if we win CAS increment insert count and return 1

                            return 1;
                        }
                        else {
                            // else check if value that was lost to is same, if
                            // not keep going, if is turn 0
                            if (compare_keys(getKey(ht->innerTable[res]),
                                             getKey(n))) {
                                return 0;
                            }
                        }
                    }
                }  // this is from the loop in ubound
            }
            // update locacur
            localEnd = head->end;
        }

        // if found no available slot in hashtable create new subtable and add
        // it to hashtable then try insertion again

        // next table size defaults to same size
        uint32_t nextTableSize =
            head->tableArray[(localEnd - 1) & (max_tables - 1)]->tableSize << 1;

        // create next subtables
        piq_subtable_t * new_table = createST(head, nextTableSize);
        addDrop(head, new_table, localEnd, n, tid, start + 1);
        start = localEnd;
    }
}
#endif  // PIQ_LAZY_RESIZE

//////////////////////////////////////////////////////////////////////
// end helpers


// api function user calls to query the table for a given node. Returns
// 1 if found, 0 otherwise.
piq_node_t *
piq_find_node(piq_ht * head, piq_key_t key, uint32_t tid) {
    piq_subtable_t * ht              = NULL;
    const uint32_t   logReadsPerLine = PIQ_DEFAULT_LOG_READS_PER_LINE;
    const uint32_t   uBound          = PIQ_DEFAULT_READS_PER_LINE;
    const uint32_t   ha              = PIQ_DEFAULT_HASH_ATTEMPTS;


    uint32_t buckets[PIQ_DEFAULT_HASH_ATTEMPTS];
    for (uint32_t i = 0; i < ha; i++) {
        buckets[i] = hashFun(key, head->seeds[i]);
    }

    uint16_t tag = hashTag(key);
    for (uint32_t j = head->start; j < head->end; j++) {
        ht = head->tableArray[j];

        uint32_t tsizeMask = ((ht->tableSize - 1) >> logReadsPerLine)
                             << logReadsPerLine;
        for (uint32_t i = 0; i < ha; i++) {

            uint32_t slot = (buckets[i] & tsizeMask);
            for (uint32_t c = tag; c < uBound + tag; c++) {
                int32_t res = lookupQuery(ht, key, slot + (c & (uBound - 1)));

                if (res == unknown_in) {  // unkown if in our not
                    continue;
                }
                if (res == not_in) {
                    return NULL; /* indicate not found */
                }
                return (frame_node_t *)getNodePtr(ht->innerTable[res]);
            }
        }
    }
    // was never found
    return NULL;
}


// insert a new node into the table. Returns 0 if node is already present, 1
// otherwise.
piq_node_t *
piq_add_node(piq_ht * head, piq_node_t * n, uint32_t tid) {
    // use local max for adddroping subtables (otherwise race condition
    // where 2 go to add to slot n, one completes addition and increments
    // head->end and then second one goes and double adds. Won't affect
    // correctness but best not have that happen.


    const uint32_t logReadsPerLine = PIQ_DEFAULT_LOG_READS_PER_LINE;
    const uint32_t uBound          = PIQ_DEFAULT_READS_PER_LINE;
    const uint32_t ha              = PIQ_DEFAULT_HASH_ATTEMPTS;

    uint32_t buckets[PIQ_DEFAULT_HASH_ATTEMPTS];
    for (uint32_t i = 0; i < ha; i++) {
        buckets[i] = hashFun(getKey(n), head->seeds[i]);
    }

    uint16_t         tag      = hashTag(getKey(n));
    uint32_t         localEnd = head->end;
    uint32_t         start    = head->start;
    piq_subtable_t * ht       = NULL;
    while (1) {
        // iterate through subtables
        // again is mod max_subtables
        for (uint32_t j = start; j < head->end; j++) {
            ht                 = head->tableArray[j];
            uint32_t tsizeMask = ((ht->tableSize - 1) >> logReadsPerLine)
                                 << logReadsPerLine;


            // do copy if there is a new bigger subtable and
            // currently in smallest subtable
#ifdef PIQ_LAZY_RESIZE
            uint32_t doCopy = (j == (head->start & (max_tables - 1))) &&
                              (head->end - head->start > RESIZE_THRESHOLD);
#endif  // PIQ_LAZY_RESIZE

            // iterate through hash functions
            for (uint32_t i = 0; i < ha; i++) {

                uint32_t slot = (buckets[i] & tsizeMask);
                for (uint32_t c = tag; c < uBound + tag; c++) {
                    int32_t res = lookup(head,
                                         ht,
                                         n,
                                         slot + (c & (uBound - 1)),
                                         tid
#ifdef PIQ_LAZY_RESIZE
                                         ,
                                         doCopy,
                                         j
#endif  // PIQ_LAZY_RESIZE
                    );
                    // lookup value in sub table

                    if (res == unknown_in) {  // unkown if in our not
                        continue;
                    }
                    else if (res == is_in) {  // is in
                        return set_return(
                            ht->innerTable[slot + (c & (uBound - 1))],
                            1);
                    }

                    else {
                        // start with absolutely known
                        // place for safe next bit
                        // calculation note that has to
                        // be before CAS otherwise can
                        // create race condition (can
                        // for example cause setCopy to
                        // fail spuriously)
                        // next hash stuff here
#ifdef PIQ_NEXT_HASH
                        uint16_t next_bits =
                            createNHMask(buckets[0], ht->logTableSize);
                        setNH(n, next_bits);
#endif  // PIQ_NEXT_HASH

                        // if return was null (is
                        // available slot in sub table)
                        // try and add with CAS. if
                        // succeed return 1, else if
                        // value it lost to is item
                        // itself return. If neither
                        // continue trying to add
                        piq_node_t * expected = NULL;
                        uint32_t     cmp =
                            __atomic_compare_exchange((ht->innerTable + res),
                                                      &expected,
                                                      &n,
                                                      1,
                                                      __ATOMIC_RELAXED,
                                                      __ATOMIC_RELAXED);
                        if (cmp) {
                            // if we win CAS
                            // increment insert count
                            // and return 1
                            return set_return(ht->innerTable[res], 1);
                        }
                        else {
                            // else check if value
                            // that was lost to is
                            // same, if not keep
                            // going, if is turn 0
                            if (
#ifdef PIQ_MARK_NULL
                                getNodePtr(ht->innerTable[res]) &&
#endif  // PIQ_MARK_NULL
                                compare_keys(getKey(ht->innerTable[res]),
                                             getKey(n))) {
                                return set_return(ht->innerTable[res], 0);
                            }
                        }
                    }
                }  // this is from the loop in ubound
            }
            // update locacur
            localEnd = head->end;
        }

        // if found no available slot in hashtable create new subtable
        // and add it to hashtable then try insertion again

        // next table size defaults to same size
        uint32_t nextTableSize =
            head->tableArray[(localEnd - 1) & (max_tables - 1)]->tableSize << 1;

        // create next subtables
        piq_subtable_t * new_table = createST(head, nextTableSize);
        addDrop(head, new_table, localEnd, n, tid, start + 1);
        start = localEnd;
    }
}

// initial hashtable. First table head will be null, after that will
// just reinitialize first table returns a pointer to the hashtable
piq_ht *
piq_init_table() {

    piq_ht * head = (piq_ht *)myacalloc(L1_CACHE_LINE_SIZE, 1, sizeof(piq_ht));

    head->tableArray = (piq_subtable_t **)myacalloc(L1_CACHE_LINE_SIZE,
                                                    max_tables,
                                                    sizeof(piq_subtable_t *));

    head->tableArray[0] = createST(head, PIQ_DEFAULT_INIT_TSIZE);
    head->end           = 1;
    head->start         = 0;

    for (uint32_t i = 0; i < PIQ_DEFAULT_HASH_ATTEMPTS; i++) {
        uint32_t rand_var = rand();
        memcpy((void *)(((uint64_t)head) + offsetof(piq_ht, seeds) +
                        (i * sizeof(uint32_t))),
               &rand_var,
               sizeof(rand_var));
    }
    return head;
}


void
piq_free_table(piq_ht * head, void (*free_ent)(void *)) {

    uint32_t count = 0;
    for (uint32_t i = 0; i < head->end; i++) {
        piq_subtable_t * ht = head->tableArray[i];
        for (uint32_t j = 0; j < ht->tableSize; j++) {
            if (getNodePtr(ht->innerTable[j]) != NULL) {
                if (!getCopy(ht->innerTable[j])) {
                    count++;
                    free_ent((void *)getVal(ht->innerTable[j]));

                    free(getNodePtr(ht->innerTable[j]));
                }
            }
        }
        freeST(ht);
    }
    free(head->tableArray);
    free(head);
}
