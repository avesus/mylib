#ifndef _DEBUG_H_
#define _DEBUG_H_

// must be power of 2. For truly race free code N_FRAMES >= max number
// of parallel frames that can be added to a given ID. For example 1000
// threads, using ID as ID N_FRAMES = 1 works, 1000 threads all going
// to ID = 0, N_FRAMES must be >= 1024
#define FDBG_N_FRAMES 8

// for clean printing. Variables with name > PRINT_ALIGN chars will be an issue
#define FDBG_PRINT_ALIGN 24

// low bit 0x1 means >= N_FRAMES have been set (this is necessary for
// indexing
#define FDBG_WRAPPED 0x1

// active means another thread is currently operating on frames
// struct. This will throw and error.
#define FDBG_ACTIVE 0x2

#include <helpers/bits.h>
#include <helpers/util.h>
#include <local/hashtable.h>
#include <local/macro-helper.h>

void init_debugger();
void free_debugger();

typedef struct frame_data {
    uint32_t nargs;
    uint32_t format_len;
    uint32_t var_name_len;
    uint32_t var_size_len;
    uint32_t size_align;
    uint32_t data_align;
    uint32_t line_num;
    uint64_t ID;
    char     data[];
} frame_data_t;

#define FDBG_META_DATA_SIZE offsetof(frame_data_t, data)


#define FDBG_ALIGNMENT      sizeof(uint64_t)  // for now this should be enough
#define FDBG_ALIGNMENT_MASK (FDBG_ALIGNMENT - 1)  // Mask of above


// helper to determine if it is var or region argument. A by-product of
// this method (couldn't find any others) is that a variable CANNOT
// start with parent, i.e passing (a + 1) would break things, passing a
//+ (1 + 2) would be fine.
#define FDBG_M_CHECK(...)        FDBG_M_CHECK_(__VA_ARGS__)
#define FDBG_M_CHECK_(a, b, ...) b
#define FDBG_M_IS_PAREN(x)       FDBG_M_CHECK(M_IS_PAREN_ x, VAR)
#define FDBG_M_IS_PAREN_(...)    FDBG_REG, FDBG_REG
#define FDBG_M_CONCAT(a, b)      FDBG_M_CONCAT_(a, b)
#define FDBG_M_CONCAT_(a, b)     a##b
#define FDBG_DEF(x)              FDBG_M_IS_PAREN(x)

// if var we need to add argument for its size, if region size should
// have already be specified
#define FDBG_DEF_VAR(X) X, sizeof(X)
#define FDBG_DEF_REG(X) X

#define FDBG_COND_DEF(X)       FDBG_M_CONCAT(FDBG_DEF_, FDBG_DEF(X))(X)
#define FDBG_COND_WRAPPER(...) APPLY(FDBG_COND_DEF, __VA_ARGS__)


// sum up all data sizes
#define FDBG_TOTAL_GET_SIZE(...) SUM_X_N_Y(FDBG_GET_SIZE, __VA_ARGS__)

// get size of all data
#define FDBG_GET_SIZE(X, Y) FDBG_ROUNDUP_AL(Y)

// helper to get size for all and sum
#define FDBG_TOTAL_TYPE_LEN(...) SUM(FDBG_TO_SIZEOF, __VA_ARGS__)

// get sizeof strings (this is used on format/var names)
#define FDBG_TO_SIZEOF(X) sizeof(X)


// sum size of variable names/format specifiers passed
#define FDBG_TO_STRLEN(X)       sizeof(X)
#define FDBG_TOTAL_STR_LEN(...) SUM(FDBG_TO_STRLEN, __VA_ARGS__)

// to get names of all variables passed as strings
#define FDBG_TO_NAME(X, Y)  #X
#define FDBG_VAR_NAMES(...) APPLY_X_N_Y(FDBG_TO_NAME, __VA_ARGS__)

// creates buffer to store all frame data
#define FDBG_CREATE_BUFFER(ln, X)                                              \
    mycalloc((X + FDBG_META_DATA_SIZE), sizeof(char));

// rounds up a size.
#define FDBG_ROUNDUP_AL(X)                                                     \
    (((X) + (FDBG_ALIGNMENT_MASK)) & (~(FDBG_ALIGNMENT_MASK)))

// for aligning so that data types are on DBG_ALIGNMENT byte boundary (can be
// important in some cases)
#define FDBG_ALIGN(X) (FDBG_ALIGNMENT - ((X) & (FDBG_ALIGNMENT_MASK)));

// copy strings helper
#define FDBG_VAR_COPY_STR(X, ...) APPLY_X(FDBG_TO_COPY_STR, X, __VA_ARGS__)

// copy string data. Assumed all strings are constant so can optimize
// with sizeof() instead of strlen. This is only used on format
// string/variable names
#define FDBG_TO_COPY_STR(X, Y)                                                 \
    memcpy((X), (Y), (sizeof(Y)));                                             \
    ((X) += sizeof((Y)))


// helpers for copying either region or var. If var just cast and set,
// else memcpy (we want adding a frame to be low weight)
#define FDBG_C_CHECK(...)              FDBG_C_CHECK_(__VA_ARGS__)
#define FDBG_C_CHECK_(a, b, c, d, ...) d
#define FDBG_C_COND_DEF(X, Y, Z)                                               \
    FDBG_M_CONCAT(FDBG_DEF_, FDBG_C_DEF(Y, X, Z))(X, Y, Z)
#define FDBG_C_DEF(x, y, z)      FDBG_C_IS_PAREN(x, y, z)
#define FDBG_C_IS_PAREN(x, y, z) FDBG_C_CHECK(C_IS_PAREN_ x, y, z, TO_COPY_VAL)
#define FDBG_C_IS_PAREN_(...)    TO_COPY_REG, TO_COPY_REG, TO_COPY_REG, TO_COPY_REG
#define FDBG_COPY_VARS(X, ...)   APPLY_X_Y_Z(FDBG_C_COND_DEF, X, __VA_ARGS__)

// copy region
#define FDBG_DEF_TO_COPY_REG(X, Y, Z)                                          \
    memcpy((X), (Y), (Z));                                                     \
    (X) += FDBG_ROUNDUP_AL(Z)


// gcc has typeof, c++ equivilent is decltype
#ifdef CPP_MODE

// copy variable
#define FDBG_DEF_TO_COPY_VAL(X, Y, Z)                                          \
    *(decltype(Y) *)(X) = (Y);                                                 \
    (X) += FDBG_ROUNDUP_AL(Z)

#elif defined C_MODE

// copy variable
#define FDBG_DEF_TO_COPY_VAL(X, Y, Z)                                          \
    *(typeof(Y) *)(X) = (Y);                                                   \
    (X) += FDBG_ROUNDUP_AL(Z)


#endif


// copy sizeof data helper
#define FDBG_COPY_SIZES(X, ...) APPLY_X_Y_Z(FDBG_TO_COPY_SIZE, X, __VA_ARGS__)
// copy sizeof type (could probably make this shorts, regions can
// potentially be big though
#define FDBG_TO_COPY_SIZE(X, Y, Z)                                             \
    *((uint32_t *)(X)) = (Z);                                                  \
    (X) += sizeof(uint32_t)


// stores the frame symbols. In macro because alot of the information
// becomes lost/harder to find once passed to function. An idea is to
// reused buffers already allocated for the symbol frame to be over
// written. The pro is it will be faster the con is it requires some
// locking for parallelism. Best thing to do would be rewrite malloc.
#define FDBG_NARGS       MACRO_COMBINE(restricted_do_not_use_nargs, ln)
#define FDBG_FMT_LEN     MACRO_COMBINE(restricted_do_not_use_fmt_len, ln)
#define FDBG_NAME_LEN    MACRO_COMBINE(restricted_do_not_use_name_len, ln)
#define FDBG_VAR_LEN     MACRO_COMBINE(restricted_do_not_use_var_len, ln)
#define FDBG_FILE_LEN    MACRO_COMBINE(restricted_do_not_use_file_len, ln)
#define FDBG_FUN_LEN     MACRO_COMBINE(restricted_do_not_use_fun_len, ln)
#define FDBG_SIZE_LEN    MACRO_COMBINE(restricted_do_not_use_size_len, ln)
#define FDBG_SIZE_ALIGN  MACRO_COMBINE(restricted_do_not_use_size_align_len, ln)
#define FDBG_DATA_ALIGN  MACRO_COMBINE(restricted_do_not_use_data_align_len, ln)
#define FDBG_TOTAL_SIZE  MACRO_COMBINE(restricted_do_not_use_total_size, ln)
#define FDBG_STRUCT_PTR  MACRO_COMBINE(restricted_do_not_use_struct_ptr, ln)
#define FDBG_BUF_PTR     MACRO_COMBINE(restricted_do_not_use_buf_ptr, ln)
#define FDBG_ID_NODE     MACRO_COMBINE(restricted_do_not_use_id_node, ln)
#define FDBG_ID_STRUCT   MACRO_COMBINE(restricted_do_not_use_id_struct, ln)
#define FDBG_FRAME_INDEX MACRO_COMBINE(restricted_do_not_use_frame_index, ln)

//////////////////////////////////////////////////////////////////////
// macros that are actually part of API

// stores the frame symbols. In macro because alot of the information
// becomes lost/harder to find once passed to function
#define FDBG_FRAME_AT_POINT(f, fn, ln, IDN, F, V)                              \
    {                                                                          \
        do {                                                                   \
            assert(2 * PP_NARG(F) == PP_NARG(V));                              \
            const uint32_t FDBG_NARGS   = PP_NARG(F);                          \
            const uint32_t FDBG_FMT_LEN = FDBG_TOTAL_STR_LEN(F);               \
            const uint32_t FDBG_NAME_LEN =                                     \
                FDBG_TOTAL_STR_LEN(FDBG_VAR_NAMES(V));                         \
            const uint32_t FDBG_VAR_LEN  = FDBG_TOTAL_GET_SIZE(V);             \
            const uint32_t FDBG_FILE_LEN = sizeof(f);                          \
            const uint32_t FDBG_FUN_LEN  = sizeof(fn);                         \
            const uint32_t FDBG_SIZE_LEN = (FDBG_NARGS * sizeof(uint32_t));    \
            const uint32_t FDBG_SIZE_ALIGN =                                   \
                FDBG_ALIGN(FDBG_META_DATA_SIZE + FDBG_FILE_LEN +               \
                           FDBG_FUN_LEN + FDBG_FMT_LEN + FDBG_NAME_LEN);       \
            const uint32_t FDBG_DATA_ALIGN =                                   \
                FDBG_ALIGN(FDBG_META_DATA_SIZE + FDBG_FILE_LEN +               \
                           FDBG_FUN_LEN + FDBG_FMT_LEN + FDBG_NAME_LEN +       \
                           FDBG_SIZE_ALIGN + FDBG_SIZE_LEN);                   \
            const uint32_t FDBG_TOTAL_SIZE =                                   \
                FDBG_FILE_LEN + FDBG_FUN_LEN + FDBG_FMT_LEN + FDBG_VAR_LEN +   \
                FDBG_NAME_LEN + FDBG_META_DATA_SIZE + FDBG_SIZE_ALIGN +        \
                FDBG_DATA_ALIGN + FDBG_SIZE_LEN;                               \
                                                                               \
            DBG_ASSERT((FDBG_TOTAL_SIZE % FDBG_ALIGNMENT) == 0,                \
                       "Unaligned Size (%d)\n",                                \
                       FDBG_TOTAL_SIZE);                                       \
                                                                               \
            uint8_t *    FDBG_BUF_PTR = NULL;                                  \
            piq_node_t * FDBG_ID_NODE = (piq_node_t *)find_frame(IDN);         \
            if (!FDBG_ID_NODE) {                                               \
                FDBG_ID_NODE = (piq_node_t *)new_id_get(IDN);                  \
            }                                                                  \
            uint32_t FDBG_FRAME_INDEX = high_bits_get(FDBG_ID_NODE->val);      \
                                                                               \
            if (FDBG_FRAME_INDEX == FDBG_N_FRAMES) {                           \
                low_bits_set_OR(FDBG_ID_NODE->val, FDBG_WRAPPED);              \
            }                                                                  \
            FDBG_FRAME_INDEX &= (FDBG_N_FRAMES - 1);                           \
                                                                               \
            high_bits_set_INCR(FDBG_ID_NODE->val);                             \
            frame_data_t ** FDBG_ID_STRUCT =                                   \
                (frame_data_t **)get_ptr(FDBG_ID_NODE->val);                   \
                                                                               \
            DBG_ASSERT(                                                        \
                low_bits_set_XOR_atomic(FDBG_ID_NODE->val, FDBG_ACTIVE) &      \
                    FDBG_ACTIVE,                                               \
                "Duplicate ID(%lu)\n",                                         \
                IDN);                                                          \
                                                                               \
            if (FDBG_ID_STRUCT[FDBG_FRAME_INDEX] && 0 &&                       \
                (GET_MALLOC_BLOCK_LENGTH(FDBG_ID_STRUCT[FDBG_FRAME_INDEX]) >=          \
                 FDBG_TOTAL_SIZE)) {                                           \
                FDBG_BUF_PTR = (uint8_t *)FDBG_ID_STRUCT[FDBG_FRAME_INDEX];    \
            }                                                                  \
            else {                                                             \
                FDBG_BUF_PTR =                                                 \
                    (uint8_t *)FDBG_CREATE_BUFFER(ln, FDBG_TOTAL_SIZE);        \
                myfree(FDBG_ID_STRUCT[FDBG_FRAME_INDEX]);                      \
                FDBG_ID_STRUCT[FDBG_FRAME_INDEX] =                             \
                    (frame_data_t *)FDBG_BUF_PTR;                              \
            }                                                                  \
                                                                               \
            frame_data_t * FDBG_STRUCT_PTR = (frame_data_t *)FDBG_BUF_PTR;     \
            FDBG_BUF_PTR += FDBG_META_DATA_SIZE;                               \
            memcpy(FDBG_BUF_PTR, f, FDBG_FILE_LEN);                            \
            FDBG_BUF_PTR += FDBG_FILE_LEN;                                     \
            memcpy(FDBG_BUF_PTR, fn, FDBG_FUN_LEN);                            \
            FDBG_BUF_PTR += FDBG_FUN_LEN;                                      \
            FDBG_VAR_COPY_STR(FDBG_BUF_PTR, F);                                \
            FDBG_VAR_COPY_STR(FDBG_BUF_PTR, FDBG_VAR_NAMES(V));                \
            FDBG_BUF_PTR += FDBG_SIZE_ALIGN;                                   \
            FDBG_COPY_SIZES(FDBG_BUF_PTR, V);                                  \
            FDBG_BUF_PTR += FDBG_DATA_ALIGN;                                   \
            FDBG_COPY_VARS(FDBG_BUF_PTR, V);                                   \
            FDBG_STRUCT_PTR->nargs        = (uint32_t)FDBG_NARGS;              \
            FDBG_STRUCT_PTR->format_len   = (uint32_t)FDBG_FMT_LEN;            \
            FDBG_STRUCT_PTR->var_name_len = (uint32_t)FDBG_NAME_LEN;           \
            FDBG_STRUCT_PTR->var_size_len = (uint32_t)FDBG_VAR_LEN;            \
            FDBG_STRUCT_PTR->line_num     = (uint32_t)ln;                      \
            FDBG_STRUCT_PTR->ID           = (uint32_t)IDN;                     \
            FDBG_STRUCT_PTR->data_align   = (uint32_t)FDBG_DATA_ALIGN;         \
            FDBG_STRUCT_PTR->size_align   = (uint32_t)FDBG_SIZE_ALIGN;         \
            assert(FDBG_TOTAL_SIZE ==                                          \
                   (uint64_t)FDBG_BUF_PTR - (uint64_t)FDBG_STRUCT_PTR);        \
                                                                               \
            DBG_ASSERT(                                                        \
                !(low_bits_set_XOR_atomic(FDBG_ID_NODE->val, FDBG_ACTIVE) &    \
                  FDBG_ACTIVE),                                                \
                "Duplicate ID(%lu)\n",                                         \
                IDN);                                                          \
                                                                               \
                                                                               \
        } while (0);                                                           \
    }

//////////////////////////////////////////////////////////////////////
// API MACROS


// All formats should be wrapped in FMT(fmt1, fmt2, fmt3....). Likewise for
// variables/regions
#define FDBG_VARS(...) __VA_ARGS__
#define FDBG_FMTS(...) __VA_ARGS__

// A region must be wrapped in REG(ptr, size of region).
#define FDBG_REG(X, Y) (X), (Y)

// end api
//////////////////////////////////////////////////////////////////////

#ifdef FRAME_DEBUGGER

// Init/deinit functions
#define FDBG_INIT_DEBUGGER init_debugger()
#define FDBG_FREE_DEBUGGER free_debugger()


#define FDBG_PRINT_FRAME_N(FRAMES, N) print_frame_n(FRAMES, N)
#define FDBG_GET_NFRAMES(FRAMES)      get_n_frames(FRAMES)

#define FDBG_PRINT_FRAMES print_frames(pthread_self())
#define FDBG_GET_FRAMES   get_frames(pthread_self())
#define FDBG_HAS_FRAMES   check_frames(pthread_self())
#define FDBG_RESET_FRAMES reset_frames(pthread_self())
#define FDBG_NEW_FRAME(F, V)                                                   \
    FDBG_FRAME_AT_POINT(__FILE__,                                              \
                        __func__,                                              \
                        __LINE__,                                              \
                        pthread_self(),                                        \
                        FDBG_FMTS(F),                                          \
                        FDBG_VARS(FDBG_COND_WRAPPER(V)));

#define FDBG_PRINT_FRAMES_ID(ID) print_frames(ID)
#define FDBG_GET_FRAMES_ID(ID)   get_frames(ID)
#define FDBG_HAS_FRAMES_ID(ID)   check_frames(ID)
#define FDBG_RESET_FRAMES_ID(ID) reset_frames(ID)
#define FDBG_NEW_FRAME_ID(ID, F, V)                                            \
    FDBG_FRAME_AT_POINT(__FILE__,                                              \
                        __func__,                                              \
                        __LINE__,                                              \
                        ID,                                                    \
                        FDBG_FMTS(F),                                          \
                        FDBG_VARS(FDBG_COND_WRAPPER(V)));
#else

#define FDBG_INIT_DEBUGGER
#define FDBG_FREE_DEBUGGER

#define FDBG_PRINT_FRAME_N(FRAMES, N)
#define FDBG_GET_NFRAMES(FRAMES) 0

#define FDBG_PRINT_FRAMES
#define FDBG_GET_FRAMES
#define FDBG_HAS_FRAMES   0
#define FDBG_RESET_FRAMES 0
#define FDBG_NEW_FRAME(F, V)


#define FDBG_PRINT_FRAMES_ID(ID)
#define FDBG_GET_FRAMES_ID(ID)
#define FDBG_HAS_FRAMES_ID(ID)   0
#define FDBG_RESET_FRAMES_ID(ID) 0
#define FDBG_NEW_FRAME_ID(ID, F, V)

#endif

//////////////////////////////////////////////////////////////////////
// API functions
void            print_frame(frame_data_t * frame_data_s, uint32_t index);
void            add_frame(uint64_t ID, frame_data_t * frame_data_s);
void            print_frames(uint64_t ID);
void            free_frames(void * ptr);
uint32_t        check_frames(uint64_t ID);
uint32_t        reset_frames(uint64_t ID);
frame_data_t ** get_frames(uint64_t ID);
void            print_frame_n(frame_data_t ** frames, uint32_t frame_number);
uint32_t        get_n_frames(frame_data_t ** frames);
void *          find_frame(uint64_t ID);
void *          new_id_get(uint64_t ID);



#endif
