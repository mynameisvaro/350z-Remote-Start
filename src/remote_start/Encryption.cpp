// Software optimized 48-bit Philips/NXP Mifare Hitag2 PCF7936/46/47/52 stream cipher algorithm by I.C. Wiener 2006-2007.
// For educational purposes only.
// No warranties or guarantees of any kind.
// This code is released into the public domain by its author.
#include <stdint.h>
#include <stdio.h>
#include "Encryption.h"

// Basic macros:

#define rev8(x) ((((x) >> 7) & 1) + ((((x) >> 6) & 1) << 1) + ((((x) >> 5) & 1) << 2) + ((((x) >> 4) & 1) << 3) + ((((x) >> 3) & 1) << 4) + ((((x) >> 2) & 1) << 5) + ((((x) >> 1) & 1) << 6) + (((x) & 1) << 7))
#define rev16(x) (rev8(x) + (rev8(x >> 8) << 8))
#define rev32(x) (rev16(x) + (rev16(x >> 16) << 16))
#define rev64(x) (rev32(x) + (rev32(x >> 32) << 32))
#define bit(x, n) (((x) >> (n)) & 1)
#define bit32(x, n) ((((x)[(n) >> 5]) >> ((n))) & 1)
#define inv32(x, i, n) ((x)[(i) >> 5] ^= ((uint32_t)(n)) << ((i) & 31))
#define rotl64(x, n) ((((uint64_t)(x)) << ((n) & 63)) + (((uint64_t)(x)) >> ((0 - (n)) & 63)))
#define reverseHex32(x) (((0xFF & x) << 24) | ((0xFF00 & x) << 8) | ((0xFF0000 & x) >> 8) | ((0xFF000000 & x) >> 24))
#define reverseHexKey(x) (((0xFF & x) << 40) | ((0xFF00 & x) << 24) | ((0xFF0000 & x) << 8) | ((0xFF000000 & x) >> 8) | ((0xFF00000000 & x) >> 24) | (0xFF0000000000 & x) >> 40)

// Single bit Hitag2 functions:

#define i4(x, a, b, c, d) ((uint32_t)((((x) >> (a)) & 1) + (((x) >> (b)) & 1) * 2 + (((x) >> (c)) & 1) * 4 + (((x) >> (d)) & 1) * 8))

static const uint32_t ht2_f4a = 0x2C79;     // 0010 1100 0111 1001
static const uint32_t ht2_f4b = 0x6671;     // 0110 0110 0111 0001
static const uint32_t ht2_f5c = 0x7907287B; // 0111 1001 0000 0111 0010 1000 0111 1011

static uint32_t f20(const uint64_t x)
{
    uint32_t i5;

    i5 = ((ht2_f4a >> i4(x, 1, 2, 4, 5)) & 1) * 1 + ((ht2_f4b >> i4(x, 7, 11, 13, 14)) & 1) * 2 + ((ht2_f4b >> i4(x, 16, 20, 22, 25)) & 1) * 4 + ((ht2_f4b >> i4(x, 27, 28, 30, 32)) & 1) * 8 + ((ht2_f4a >> i4(x, 33, 42, 43, 45)) & 1) * 16;

    return (ht2_f5c >> i5) & 1;
}

static uint64_t hitag2_init(const uint64_t key, const uint32_t serial, const uint32_t IV)
{
    uint32_t i;
    uint64_t x = ((key & 0xFFFF) << 32) + serial;

    for (i = 0; i < 32; i++)
    {
        x >>= 1;
        x += (uint64_t)(f20(x) ^ (((IV >> i) ^ (key >> (i + 16))) & 1)) << 47;
    }
    return x;
}

static uint64_t hitag2_round(uint64_t *state)
{
    uint64_t x = *state;

    x = (x >> 1) +
        ((((x >> 0) ^ (x >> 2) ^ (x >> 3) ^ (x >> 6) ^ (x >> 7) ^ (x >> 8) ^ (x >> 16) ^ (x >> 22) ^ (x >> 23) ^ (x >> 26) ^ (x >> 30) ^ (x >> 41) ^ (x >> 42) ^ (x >> 43) ^ (x >> 46) ^ (x >> 47)) & 1) << 47);

    *state = x;
    return f20(x);
}

static uint32_t hitag2_byte(uint64_t *x, uint8_t size)
{
    uint32_t i, c;

    for (i = 0, c = 0; i < size; i++)
        c += (uint32_t)hitag2_round(x) << (i ^ 7);
    return c;
}

// Main function
uint32_t getKS(uint64_t KEY, uint32_t UID, uint32_t IV)
{

    uint32_t i;
    uint64_t state;
    uint32_t page3KS = 0;
    uint32_t data[16];
    state = hitag2_init(rev64(reverseHexKey(KEY)), rev32(reverseHex32(UID)), rev32(reverseHex32(IV)));

    // 32 bits of 0xFFFFFFFF (not used)
    for (i = 0; i < 4; i++)
        hitag2_byte(&state, 8);

    // Page 3
    for (i = 0; i < 4; i++)
        page3KS |= (hitag2_byte(&state, 8) << (24 - (8 * i)));

    return page3KS;
}
