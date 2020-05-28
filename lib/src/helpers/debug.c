#include <helpers/debug.h>


static piq_ht * global_debug_table = NULL;

// initializes new debugger
void
init_debugger() {
    global_debug_table = piq_init_table();
}

// frees existing debugger
void
free_debugger() {
    piq_free_table(global_debug_table, free_frames);
}


#define FDBG_PTR_ADD(Z, X, Y) (Z)(((uint64_t)(X)) + (Y))

// helper to avoid reusing same code. Basically creates the buffer to
// be printed/sets size of type for type (X)
#define FDBG_HANDLE_TYPE(X)                                                    \
    {                                                                          \
        s_incr = sizeof(X);                                                    \
        assert(s_incr + s <= var_sizes[i]);                                    \
        sprintf(print_buf,                                                     \
                format_buf,                                                    \
                *((X *)(FDBG_PTR_ADD(void *, var_data, s))));                  \
    }


// prints a given frame. for formats that take multiple types like %G
// and %g, capitol is double, lowercase is float
void
print_frame(frame_data_t * frame_data_s, uint32_t index) {
    uint32_t nargs        = frame_data_s->nargs;
    uint32_t format_len   = frame_data_s->format_len;
    uint32_t var_name_len = frame_data_s->var_name_len;

    // seperate ptr for all unique regions in data buffer
    char *     file_name   = frame_data_s->data;
    char *     func_name   = frame_data_s->data + strlen(file_name) + 1;
    char *     format_data = func_name + strlen(func_name) + 1;
    char *     name_data   = format_data + format_len;
    uint32_t * var_sizes =
        (uint32_t *)(name_data + var_name_len + frame_data_s->size_align);

    void * var_data =
        FDBG_PTR_ADD(void *, (var_sizes + nargs), frame_data_s->data_align);

    char format_buf[MED_BUF_LEN] = "", print_buf[MED_BUF_LEN] = "";

    // just to make printing a bit fancies
    memset(format_buf, '-', (FDBG_PRINT_ALIGN - strlen("START") + 1) / 2);
    memcpy(format_buf + (FDBG_PRINT_ALIGN - strlen("START") + 1) / 2,
           "START",
           strlen("START"));
    memset(format_buf + (FDBG_PRINT_ALIGN + strlen("START") + 1) / 2,
           '-',
           (FDBG_PRINT_ALIGN - strlen("START") + 1) / 2);


    // print file/function/set header
    sprintf(print_buf,
            "%*s %s:%s:%d: ID(%lu) idx(%d/%d): %*s\n",
            FDBG_PRINT_ALIGN,
            format_buf,
            file_name,
            func_name,
            frame_data_s->line_num,
            frame_data_s->ID,
            index,
            FDBG_N_FRAMES - 1,
            FDBG_PRINT_ALIGN,
            format_buf);
    fprintf(stderr, "%s", print_buf);

    // print all symbols
    for (uint32_t i = 0; i < nargs; i++) {

        // ive had a few issues with these buffers containiner un initilized
        // memory (I had thought sprintf 0d). Given that this is not on
        // critical path I dont see a huge issue with this but a better method
        // would be nice.
        memset(format_buf, 0, MED_BUF_LEN);
        memset(print_buf, 0, MED_BUF_LEN);
        uint32_t format_str_len = strlen(format_data);
        uint32_t name_str_len   = strlen(name_data);

        // create format string for symbols
        sprintf(format_buf,
                "\t%s"
                "%*s: %s",
                name_data,
                FDBG_PRINT_ALIGN - name_str_len,
                "",
                format_data);


        // iterate through size blocks relating to the format type. For a
        // single variable this just does one print, for regions this will
        // iterate through the region.
        uint32_t s = 0, s_incr = 0;
        for (; s < var_sizes[i];) {
            // we would want switch here but cant really do that cuz need
            // strcmp. The reason we need this is we need to know how to
            // interpret the bytes of string (mostly this is just int vs
            // float).
            if (!strcmp(format_data, "%c")) {
                FDBG_HANDLE_TYPE(char);
            }
            else if (!strcmp(format_data, "%d")) {
                FDBG_HANDLE_TYPE(int);
            }
            else if (!strcmp(format_data, "%e")) {
                FDBG_HANDLE_TYPE(float);
            }
            else if (!strcmp(format_data, "%E")) {
                FDBG_HANDLE_TYPE(double);
            }
            else if (!strcmp(format_data, "%f")) {
                FDBG_HANDLE_TYPE(float);
            }
            else if (!strcmp(format_data, "%g")) {
                FDBG_HANDLE_TYPE(float);
            }
            else if (!strcmp(format_data, "%G")) {
                FDBG_HANDLE_TYPE(double);
            }
            else if (!strcmp(format_data, "%hi")) {
                FDBG_HANDLE_TYPE(short);
            }
            else if (!strcmp(format_data, "%hu")) {
                FDBG_HANDLE_TYPE(unsigned short);
            }
            else if (!strcmp(format_data, "%i")) {
                FDBG_HANDLE_TYPE(int);
            }
            else if (!strcmp(format_data, "%l")) {
                FDBG_HANDLE_TYPE(long);
            }
            else if (!strcmp(format_data, "%ld")) {
                FDBG_HANDLE_TYPE(long);
            }
            else if (!strcmp(format_data, "%li")) {
                FDBG_HANDLE_TYPE(long);
            }
            else if (!strcmp(format_data, "%lf")) {
                FDBG_HANDLE_TYPE(double);
            }
            else if (!strcmp(format_data, "%Lf")) {
                FDBG_HANDLE_TYPE(long double);
            }
            else if (!strcmp(format_data, "%lu")) {
                FDBG_HANDLE_TYPE(unsigned long);
            }
            else if (!strcmp(format_data, "%lli")) {
                FDBG_HANDLE_TYPE(long long);
            }
            else if (!strcmp(format_data, "%lld")) {
                FDBG_HANDLE_TYPE(long long);
            }
            else if (!strcmp(format_data, "%llu")) {
                FDBG_HANDLE_TYPE(unsigned long long);
            }
            else if (!strcmp(format_data, "%o")) {
                FDBG_HANDLE_TYPE(long);
            }
            else if (!strcmp(format_data, "%p")) {
                FDBG_HANDLE_TYPE(void *);
            }
            else if (!strcmp(format_data, "%s")) {
                s_incr = var_sizes[i];
                assert(s_incr + s <= var_sizes[i]);
                sprintf(print_buf,
                        format_buf,
                        FDBG_PTR_ADD(char *, var_data, s));
            }
            else if (!strcmp(format_data, "%u")) {
                FDBG_HANDLE_TYPE(unsigned int);
            }
            else if (!strcmp(format_data, "%x")) {
                FDBG_HANDLE_TYPE(unsigned int);
            }
            else if (!strcmp(format_data, "%X")) {
                FDBG_HANDLE_TYPE(unsigned long);
            }
            else {
                fprintf(stderr,
                        "Unsupported format at (%d) -> %s:%s\n",
                        i,
                        name_data,
                        format_data);
                exit(-1);
            }
            fprintf(stderr, "%s", print_buf);

            assert(s_incr);
            s += s_incr;
            if (s != var_sizes[i] && (s % FDBG_ALIGNMENT)) {

                // the '\n check is if its first block in region
                uint32_t prev_used =
                    strlen(print_buf +
                           (FDBG_PRINT_ALIGN + 3 + (print_buf[0] == '\n')));
                sprintf(format_buf,
                        "%*s%s",
                        (s_incr << 2) - prev_used,
                        "",
                        format_data);
            }
            else if (s != var_sizes[i]) {
                sprintf(format_buf,
                        "\n\t%s + %d"
                        "%*s: %s",
                        name_data,
                        s,
                        FDBG_PRINT_ALIGN - (name_str_len + 4 + (int)log10(s)),
                        "",
                        format_data);
            }
        }
        fprintf(stderr, "\n");

        // seperated by \0 so this works...
        // likewise + 1 is for the \0
        format_data += format_str_len + 1;
        name_data += name_str_len + 1;
        var_data =
            FDBG_PTR_ADD(void *, var_data, FDBG_ROUNDUP_AL(var_sizes[i]));
    }
    memset(format_buf + FDBG_PRINT_ALIGN, 0, 2);
    memset(format_buf, '-', (FDBG_PRINT_ALIGN - strlen("END") + 1) / 2);
    memcpy(format_buf + (FDBG_PRINT_ALIGN - strlen("END") + 1) / 2,
           "END",
           strlen("END"));
    memset(format_buf + (FDBG_PRINT_ALIGN + strlen("END") + 1) / 2,
           '-',
           (FDBG_PRINT_ALIGN - strlen("END") + 1) / 2);

    sprintf(print_buf,
            "%*s %s:%s:%d: ID(%lu) idx(%d/%d): %*s\n",
            FDBG_PRINT_ALIGN,
            format_buf,
            file_name,
            func_name,
            frame_data_s->line_num,
            frame_data_s->ID,
            index,
            FDBG_N_FRAMES - 1,
            FDBG_PRINT_ALIGN,
            format_buf);
    fprintf(stderr, "%s", print_buf);
}

// helper that acts as something of a hash function for which index in
// ID array thread should use in hashtable. Can be configured to be
// corenum (which is generally best)
uint8_t
to_8(uint64_t t) {
    t ^= t >> 32;
    t ^= t >> 16;
    t ^= t >> 8;
    return t & (0xff);
}


// adds new frame for a given ID to the hashtable
void *
new_id_get(uint64_t ID) {
    assert(global_debug_table);
    frame_data_t ** frame_list =
        (frame_data_t **)mycalloc(FDBG_N_FRAMES, sizeof(frame_data_t *));
    piq_node_t * ret    = (piq_node_t *)mycalloc(1, sizeof(piq_node_t));
    ret->key            = ID;
    ret->val            = (void *)frame_list;
    piq_node_t * ret_in = piq_add_node(global_debug_table, ret, to_8(ID));
    DBG_ASSERT(low_bits_get(ret_in),
               "Hashtable error, most likely due to duplicate ID(%lu)\n",
               ID);
    DBG_ASSERT(get_ptr(ret) == get_ptr(ret_in),
               "Hashtable error, value added not value returned\n\t"
               "Expec : %p\n\t"
               "Actual: %p\n",
               get_ptr(ret),
               get_ptr(ret_in));
    return get_ptr(ret_in);
}

void *
find_frame(uint64_t ID) {
    return piq_find_node(global_debug_table, ID, to_8(ID));
}

// adds new frame for a given ID to the hashtable
void
add_frame(uint64_t ID, frame_data_t * frame_data_s) {
    assert(global_debug_table);
    piq_node_t * ret = piq_find_node(global_debug_table, ID, to_8(ID));
    if (!ret) {
        frame_data_t ** frame_list =
            (frame_data_t **)mycalloc(FDBG_N_FRAMES, sizeof(frame_data_t *));
        ret                 = (piq_node_t *)mycalloc(1, sizeof(piq_node_t));
        ret->key            = ID;
        ret->val            = (void *)frame_list;
        piq_node_t * ret_in = piq_add_node(global_debug_table, ret, to_8(ID));
        DBG_ASSERT(low_bits_get(ret_in),
                   "Hashtable error, most likely due to duplicate ID(%lu)\n",
                   ID);
    }

    DBG_ASSERT(low_bits_set_XOR_atomic(ret->val, FDBG_ACTIVE) & FDBG_ACTIVE,
               "Duplicate ID(%lu)\n",
               ID);

    uint64_t frame_index = high_bits_get(ret->val);
    high_bits_set_INCR(ret->val);

    frame_data_t ** frame_list = (frame_data_t **)get_ptr(ret->val);
    if (frame_index == FDBG_N_FRAMES) {
        low_bits_set_OR(ret->val, FDBG_WRAPPED);
    }

    myfree(frame_list[frame_index & (FDBG_N_FRAMES - 1)]);
    frame_list[frame_index & (FDBG_N_FRAMES - 1)] = frame_data_s;

    DBG_ASSERT(!(low_bits_set_XOR_atomic(ret->val, FDBG_ACTIVE) & FDBG_ACTIVE),
               "Duplicate ID(%lu)\n",
               ID);
}

// reset the frames for a given ID (this can be useful is for example
// after a pthread_join an old ID is being reused)
uint32_t
reset_frames(uint64_t ID) {
    piq_node_t * ret = piq_find_node(global_debug_table, ID, to_8(ID));
    if (ret) {
        DBG_ASSERT(low_bits_set_XOR_atomic(ret->val, FDBG_ACTIVE) & FDBG_ACTIVE,
                   "Duplicate ID(%lu)\n",
                   ID);

        low_bits_set_AND(ret->val, FDBG_ACTIVE);
        high_bits_set(ret->val, 0);

        DBG_ASSERT(
            !(low_bits_set_XOR_atomic(ret->val, FDBG_ACTIVE) & FDBG_ACTIVE),
            "Duplicate ID(%lu)\n",
            ID);

        return 1;
    }
    return 0;
}

void
print_frame_n(frame_data_t ** frames, uint32_t frame_number) {
    if (frame_number >= FDBG_N_FRAMES) {
        PRINT(ERROR_VERBOSE,
              "Frame(%d) out of range [0 - %d]\n",
              frame_number,
              FDBG_N_FRAMES - 1);
        return;
    }
    uint32_t        wrapparound = low_bits_get(frames) & FDBG_WRAPPED;
    uint32_t        frame_index = high_bits_get(frames);
    frame_data_t ** frames_vptr = (frame_data_t **)get_ptr(frames);

    uint32_t lb = wrapparound ? frame_index : 0;
    uint32_t hb = wrapparound ? frame_index + FDBG_N_FRAMES : frame_index;
    assert(hb >= lb);
    if (lb + frame_number >= hb) {
        PRINT(ERROR_VERBOSE,
              "Frame(%d) out of range [0 - %d]\n",
              frame_number,
              (hb - lb) - 1);
        return;
    }
    print_frame(frames_vptr[((lb + frame_number) & (FDBG_N_FRAMES - 1))],
                frame_number);
}

// returns ptr to frame array struct (should only really be used by
// printFrameN()
frame_data_t **
get_frames(uint64_t ID) {
    piq_node_t * ret = piq_find_node(global_debug_table, ID, to_8(ID));
    if (ret) {
        return (frame_data_t **)(ret->val);
    }
    return NULL;
}

// get all frames for a given ID and prints them
void
print_frames(uint64_t ID) {
    piq_node_t * ret = piq_find_node(global_debug_table, ID, to_8(ID));
    if (ret) {
        int32_t wrapparound = low_bits_get(ret->val) & FDBG_WRAPPED;
        int32_t frame_index = high_bits_get(ret->val);
        DBG_ASSERT(frame_index < FDBG_N_FRAMES || wrapparound,
                   "Error size(%d) and wrap(%d)\n",
                   frame_index,
                   wrapparound);
        frame_data_t ** frames = (frame_data_t **)get_ptr(ret->val);

        int32_t lb = wrapparound ? frame_index : 0;
        int32_t hb = wrapparound ? frame_index + FDBG_N_FRAMES : frame_index;
        for (int32_t i = lb; i < hb; i++) {
            print_frame(frames[i & (FDBG_N_FRAMES - 1)], (i - lb));
        }
    }
    else {
        PRINT(ERROR_VERBOSE,
              "Couldn't find any frames to print for id(%lu)\n",
              ID);
    }
}

// free all frames in array
void
free_frames(void * ptr) {
    frame_data_t ** frames = (frame_data_t **)get_ptr(ptr);
    for (int32_t i = 0; i < FDBG_N_FRAMES; i++) {
        // try free all as indexes can be reset w.o free
        myfree(frames[i]);
    }
    myfree(frames);
}

// check if a given ID already has frames
uint32_t
check_frames(uint64_t ID) {
    return NULL != piq_find_node(global_debug_table, ID, to_8(ID));
}

uint32_t
get_n_frames(frame_data_t ** frames) {
    int32_t wrapparound = low_bits_get(frames) & FDBG_WRAPPED;
    int32_t frame_index = high_bits_get(frames);
    DBG_ASSERT(frame_index < FDBG_N_FRAMES || wrapparound,
               "Error size(%d) but no wrap bool\n",
               frame_index);
    return wrapparound ? FDBG_N_FRAMES : frame_index;
}
