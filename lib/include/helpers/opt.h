#ifndef _MY_OPT_H_
#define _MY_OPT_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>

#define MIN(x, y)        (((x) < (y) ? (x) : (y)))
#define MAX(x, y)        (((x) > (y) ? (x) : (y)))
#define ROUNDUP_P2(X, Y) (((X) + ((Y)-1)) & (~((Y)-1)))

/*
  X	x index returned from IDX_TO_X
  Y	log_2(init_size)
*/
#define X_TO_SIZE(X, Y) ((1U) << ((X) + (Y) + ((X) == 0)))

/*
  X	index value
  Y	log_2(init_size)
*/
#define IDX_TO_X(X, Y) (sizeof_bits(uint32_t) - (fl1_32_clz((X) >> (Y))))

/*
  X	index value
  Z	x index returned from IDX_TO_X
  Y	log_2(init_size)
*/
#define IDX_TO_Y(X, Y, Z) ((X) & ((1 << ((Y) + ((Z)-1) + ((Y) == 0))) - 1))

// ff -> find first, i.e ff1 of 0b10101000 would return 3
#define ff1_32_ctz(X) ((X) ? __builtin_ctz((X)) : 32)
#define ff0_32_ctz(X) ff1_32_ctz(~(X))

// ff -> find last, i.e fl1 of 0b10101000 would return 7
#define fl1_32_clz(X) ((X) ? __builtin_clz((X)) : 32)
#define fl0_32_clz(X) fl1_32_clz(~(X))

// ff -> find first, i.e ff1 of 0b10101000 would return 3
#define ff1_64_ctz(X) ((X) ? __builtin_ctzll((X)) : 64)
#define ff0_64_ctz(X) ff1_64_ctz(~(X))

// ff -> find last, i.e fl1 of 0b10101000 would return 7
#define fl1_64_clz(X) ((X) ? __builtin_clzll((X)) : 64)
#define fl0_64_clz(X) fl1_64_clz(~(X))


// ff -> find first, i.e ff1 of 0b10101000 would return 3
#define ff1_32_ctz_nz(X) __builtin_ctz((X))
#define ff0_32_ctz_nz(X) ff1_32_ctz_nz(~(X))

// ff -> find last, i.e fl1 of 0b10101000 would return 7
#define fl1_32_clz_nz(X) __builtin_clz((X))
#define fl0_32_clz_nz(X) fl1_32_clz_nz(~(X))

// ff -> find first, i.e ff1 of 0b10101000 would return 3
#define ff1_64_ctz_nz(X) __builtin_ctzll((X))
#define ff0_64_ctz_nz(X) ff1_64_ctz_nz(~(X))

// ff -> find last, i.e fl1 of 0b10101000 would return 7
#define fl1_64_clz_nz(X) __builtin_clzll((X))
#define fl0_64_clz_nz(X) fl1_64_clz_nz(~(X))


// ff -> find first, i.e ff1 of 0b10101000 would return 3
#define ff1_asm_tz(X, Y) __asm__("tzcnt %1, %0" : "=r"((Y)) : "rm"((X)));
#define ff0_asm_tz(X, Y) ff1_asm_tz(~(X), (Y))

// ff -> find last, i.e fl1 of 0b10101000 would return 7
#define fl1_asm_lz(X, Y) __asm__("lzcnt %1, %0" : "=r"((Y)) : "rm"((X)));
#define fl0_asm_lz(X, Y) fl1_asm_lz(~(X), (Y))

#define ff1_32_tz(X) _tzcnt_u32((X))
#define ff0_32_tz(X) ff1_64_tz(~(X))

// ff -> find last, i.e fl1 of 0b10101000 would return 7
#define ff1_64_tz(X) _tzcnt_u64((X))
#define fl0_64_tz(X) fl1_64_tz(~(X))

#define fl1_32_tz(X) _lzcnt_u32((X))
#define ff0_32_tz(X) ff1_64_tz(~(X))

// ff -> find last, i.e fl1 of 0b10101000 would return 7
#define fl1_64_tz(X) _lzcnt_u64((X))
#define fl0_64_tz(X) fl1_64_tz(~(X))


// ff -> find first, i.e ff1 of 0b10101000 would return 3
#define ff1_32_ffs(X) __builtin_ffs((X))
#define ff0_32_ffs(X) ff1_32_ffs(~(X))

// ff -> find first, i.e ff1 of 0b10101000 would return 3
#define ff1_64_ffs(X) __builtin_ffsll((X))
#define ff0_64_ffs(X) ff1_64_ffs(~(X))

inline uint32_t
ulog2_32(uint32_t n) {
    uint32_t s, t;
    t = (n > 0xffff) << 4;
    n >>= t;
    s = (n > 0xff) << 3;
    n >>= s, t |= s;
    s = (n > 0xf) << 2;
    n >>= s, t |= s;
    s = (n > 0x3) << 1;
    n >>= s, t |= s;
    return (t | (n >> 1));
}

inline uint32_t
ulog2_64(uint64_t n) {
    uint64_t s, t;
    t = (n > 0xffffffff) << 5;
    n >>= t;
    t = (n > 0xffff) << 4;
    n >>= t;
    s = (n > 0xff) << 3;
    n >>= s, t |= s;
    s = (n > 0xf) << 2;
    n >>= s, t |= s;
    s = (n > 0x3) << 1;
    n >>= s, t |= s;
    return (t | (n >> 1));
}


// these get optimized to popcnt
inline uint32_t
bitcount_32(uint32_t v) {
    uint32_t c;
    c = v - ((v >> 1) & 0x55555555);
    c = ((c >> 2) & 0x33333333) + (c & 0x33333333);
    c = ((c >> 4) + c) & 0x0F0F0F0F;
    c = ((c >> 8) + c) & 0x00FF00FF;
    c = ((c >> 16) + c) & 0x0000FFFF;
    return c;
}

inline uint32_t
bitcount_64(uint64_t v) {
    uint64_t c;
    c = v - ((v >> 1) & 0x5555555555555555UL);
    c = ((c >> 2) & 0x3333333333333333UL) + (c & 0x3333333333333333UL);
    c = ((c >> 4) + c) & 0x0F0F0F0F0F0F0F0FUL;
    c = ((c >> 8) + c) & 0x00FF00FF00FF00FFUL;
    c = ((c >> 16) + c) & 0x0000FFFF0000FFFFUL;
    c = ((c >> 32) + c) & 0x00000000FFFFFFFFUL;
    return c;
}

inline uint32_t
next_p2_32(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

inline uint64_t
next_p2_64(uint64_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

#define GET_MALLOC_BLOCK_LENGTH(X)                                             \
    (((uint64_t)(X))                                                           \
         ? (((*(((uint64_t *)X) - 1)) & (~MALLOC_ALIGNMENT_MASK)) -           \
            (2 * sizeof(uint64_t)))                                            \
         : 0)

#define TO_MASK_64(X) (((1UL) << (X)) - 1)
#define TO_MASK_32(X) (((1) << (X)) - 1)


#endif
