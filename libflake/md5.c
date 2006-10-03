/*
 * This is an implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm.
 *
 * Written by Solar Designer <solar at openwall.com> in 2001, and placed
 * in the public domain.  There's absolutely no warranty.
 *
 * Modified in 2006 by Justin Ruggles.
 * Still in the public domain.  Still no warranty.
 */

#include "common.h"

#include "md5.h"
#include "bswap.h"

/*
 * The basic MD5 functions.
 *
 * F is optimized compared to its RFC 1321 definition just like in Colin
 * Plumb's implementation.
 */
#define F(x, y, z)  ((z) ^ ((x) & ((y) ^ (z))))
#define G(x, y, z)  ((y) ^ ((z) & ((x) ^ (y))))
#define H(x, y, z)  ((x) ^ (y) ^ (z))
#define I(x, y, z)  ((y) ^ ((x) | ~(z)))

/*
 * The MD5 transformation for all four rounds.
 */
#define STEP(f, a, b, c, d, x, t, s) \
    (a) += f((b), (c), (d)) + (x) + (t); \
    (a) = (((a) << (s)) | (((a) & 0xFFFFFFFF) >> (32 - (s)))); \
    (a) += (b);

/*
 * SET reads 4 input bytes in little-endian byte order and stores them
 * in a properly aligned word in host byte order.
 */
#define SET(n) \
    (ctx->block[(n)] = \
    (uint32_t)ptr[(n) * 4] | \
    ((uint32_t)ptr[(n) * 4 + 1] << 8) | \
    ((uint32_t)ptr[(n) * 4 + 2] << 16) | \
    ((uint32_t)ptr[(n) * 4 + 3] << 24))

#define GET(n) \
    (ctx->block[(n)])

/*
 * This processes one or more 64-byte data blocks, but does NOT update
 * the bit counters.  There are no alignment requirements.
 */
static const void *
body(MD5Context *ctx, const void *data, uint32_t size)
{
    const uint8_t *ptr;
    uint32_t a, b, c, d;
    uint32_t saved_a, saved_b, saved_c, saved_d;

    ptr = data;

    a = ctx->a;
    b = ctx->b;
    c = ctx->c;
    d = ctx->d;

    do {
        saved_a = a;
        saved_b = b;
        saved_c = c;
        saved_d = d;

        // Round 1
        STEP(F, a, b, c, d, SET( 0), 0xD76AA478,  7)
        STEP(F, d, a, b, c, SET( 1), 0xE8C7B756, 12)
        STEP(F, c, d, a, b, SET( 2), 0x242070DB, 17)
        STEP(F, b, c, d, a, SET( 3), 0xC1BDCEEE, 22)
        STEP(F, a, b, c, d, SET( 4), 0xF57C0FAF,  7)
        STEP(F, d, a, b, c, SET( 5), 0x4787C62A, 12)
        STEP(F, c, d, a, b, SET( 6), 0xA8304613, 17)
        STEP(F, b, c, d, a, SET( 7), 0xFD469501, 22)
        STEP(F, a, b, c, d, SET( 8), 0x698098D8,  7)
        STEP(F, d, a, b, c, SET( 9), 0x8B44F7AF, 12)
        STEP(F, c, d, a, b, SET(10), 0xFFFF5BB1, 17)
        STEP(F, b, c, d, a, SET(11), 0x895CD7BE, 22)
        STEP(F, a, b, c, d, SET(12), 0x6B901122,  7)
        STEP(F, d, a, b, c, SET(13), 0xFD987193, 12)
        STEP(F, c, d, a, b, SET(14), 0xA679438E, 17)
        STEP(F, b, c, d, a, SET(15), 0x49B40821, 22)

        // Round 2
        STEP(G, a, b, c, d, GET( 1), 0xF61E2562,  5)
        STEP(G, d, a, b, c, GET( 6), 0xC040B340,  9)
        STEP(G, c, d, a, b, GET(11), 0x265E5A51, 14)
        STEP(G, b, c, d, a, GET( 0), 0xE9B6C7AA, 20)
        STEP(G, a, b, c, d, GET( 5), 0xD62F105D,  5)
        STEP(G, d, a, b, c, GET(10), 0x02441453,  9)
        STEP(G, c, d, a, b, GET(15), 0xD8A1E681, 14)
        STEP(G, b, c, d, a, GET( 4), 0xE7D3FBC8, 20)
        STEP(G, a, b, c, d, GET( 9), 0x21E1CDE6,  5)
        STEP(G, d, a, b, c, GET(14), 0xC33707D6,  9)
        STEP(G, c, d, a, b, GET( 3), 0xF4D50D87, 14)
        STEP(G, b, c, d, a, GET( 8), 0x455A14ED, 20)
        STEP(G, a, b, c, d, GET(13), 0xA9E3E905,  5)
        STEP(G, d, a, b, c, GET( 2), 0xFCEFA3F8,  9)
        STEP(G, c, d, a, b, GET( 7), 0x676F02D9, 14)
        STEP(G, b, c, d, a, GET(12), 0x8D2A4C8A, 20)

        // Round 3
        STEP(H, a, b, c, d, GET( 5), 0xFFFA3942,  4)
        STEP(H, d, a, b, c, GET( 8), 0x8771F681, 11)
        STEP(H, c, d, a, b, GET(11), 0x6D9D6122, 16)
        STEP(H, b, c, d, a, GET(14), 0xFDE5380C, 23)
        STEP(H, a, b, c, d, GET( 1), 0xA4BEEA44,  4)
        STEP(H, d, a, b, c, GET( 4), 0x4BDECFA9, 11)
        STEP(H, c, d, a, b, GET( 7), 0xF6BB4B60, 16)
        STEP(H, b, c, d, a, GET(10), 0xBEBFBC70, 23)
        STEP(H, a, b, c, d, GET(13), 0x289B7EC6,  4)
        STEP(H, d, a, b, c, GET( 0), 0xEAA127FA, 11)
        STEP(H, c, d, a, b, GET( 3), 0xD4EF3085, 16)
        STEP(H, b, c, d, a, GET( 6), 0x04881D05, 23)
        STEP(H, a, b, c, d, GET( 9), 0xD9D4D039,  4)
        STEP(H, d, a, b, c, GET(12), 0xE6DB99E5, 11)
        STEP(H, c, d, a, b, GET(15), 0x1FA27CF8, 16)
        STEP(H, b, c, d, a, GET( 2), 0xC4AC5665, 23)

        // Round 4
        STEP(I, a, b, c, d, GET( 0), 0xF4292244,  6)
        STEP(I, d, a, b, c, GET( 7), 0x432AFF97, 10)
        STEP(I, c, d, a, b, GET(14), 0xAB9423A7, 15)
        STEP(I, b, c, d, a, GET( 5), 0xFC93A039, 21)
        STEP(I, a, b, c, d, GET(12), 0x655B59C3,  6)
        STEP(I, d, a, b, c, GET( 3), 0x8F0CCC92, 10)
        STEP(I, c, d, a, b, GET(10), 0xFFEFF47D, 15)
        STEP(I, b, c, d, a, GET( 1), 0x85845DD1, 21)
        STEP(I, a, b, c, d, GET( 8), 0x6FA87E4F,  6)
        STEP(I, d, a, b, c, GET(15), 0xFE2CE6E0, 10)
        STEP(I, c, d, a, b, GET( 6), 0xA3014314, 15)
        STEP(I, b, c, d, a, GET(13), 0x4E0811A1, 21)
        STEP(I, a, b, c, d, GET( 4), 0xF7537E82,  6)
        STEP(I, d, a, b, c, GET(11), 0xBD3AF235, 10)
        STEP(I, c, d, a, b, GET( 2), 0x2AD7D2BB, 15)
        STEP(I, b, c, d, a, GET( 9), 0xEB86D391, 21)

        a += saved_a;
        b += saved_b;
        c += saved_c;
        d += saved_d;

        ptr += 64;
    } while(size -= 64);

    ctx->a = a;
    ctx->b = b;
    ctx->c = c;
    ctx->d = d;

    return ptr;
}

void
md5_init(MD5Context *ctx)
{
    ctx->a = 0x67452301;
    ctx->b = 0xefcdab89;
    ctx->c = 0x98badcfe;
    ctx->d = 0x10325476;

    ctx->lo = 0;
    ctx->hi = 0;
}

void
md5_update(MD5Context *ctx, const void *data, uint32_t size)
{
    uint32_t saved_lo;
    uint32_t used, free;

    saved_lo = ctx->lo;
    if((ctx->lo = (saved_lo + size) & 0x1FFFFFFF) < saved_lo)
        ctx->hi++;
    ctx->hi += size >> 29;

    used = saved_lo & 0x3f;

    if(used) {
        free = 64 - used;

        if(size < free) {
            memcpy(&ctx->buffer[used], data, size);
            return;
        }

        memcpy(&ctx->buffer[used], data, free);
        data = (uint8_t *)data + free;
        size -= free;
        body(ctx, ctx->buffer, 64);
    }

    if(size >= 64) {
        data = body(ctx, data, size & ~(uint32_t)0x3f);
        size &= 0x3f;
    }

    memcpy(ctx->buffer, data, size);
}

void
md5_final(uint8_t *result, MD5Context *ctx)
{
    uint32_t used, free;

    used = ctx->lo & 0x3f;

    ctx->buffer[used++] = 0x80;

    free = 64 - used;

    if(free < 8) {
        memset(&ctx->buffer[used], 0, free);
        body(ctx, ctx->buffer, 64);
        used = 0;
        free = 64;
    }

    memset(&ctx->buffer[used], 0, free - 8);

    ctx->lo <<= 3;
    ctx->buffer[56] = ctx->lo;
    ctx->buffer[57] = ctx->lo >> 8;
    ctx->buffer[58] = ctx->lo >> 16;
    ctx->buffer[59] = ctx->lo >> 24;
    ctx->buffer[60] = ctx->hi;
    ctx->buffer[61] = ctx->hi >> 8;
    ctx->buffer[62] = ctx->hi >> 16;
    ctx->buffer[63] = ctx->hi >> 24;

    body(ctx, ctx->buffer, 64);

    result[0] = ctx->a;
    result[1] = ctx->a >> 8;
    result[2] = ctx->a >> 16;
    result[3] = ctx->a >> 24;
    result[4] = ctx->b;
    result[5] = ctx->b >> 8;
    result[6] = ctx->b >> 16;
    result[7] = ctx->b >> 24;
    result[8] = ctx->c;
    result[9] = ctx->c >> 8;
    result[10] = ctx->c >> 16;
    result[11] = ctx->c >> 24;
    result[12] = ctx->d;
    result[13] = ctx->d >> 8;
    result[14] = ctx->d >> 16;
    result[15] = ctx->d >> 24;

    memset(ctx, 0, sizeof(*ctx));
}

/**
 * Run md5_update on the audio signal byte stream
 */
void
md5_accumulate(MD5Context *ctx, const void *signal, int ch, int nsamples)
{
    int data_bytes = ch * nsamples * 2;

#ifdef WORDS_BIGENDIAN
    int i;
    uint16_t *sig16 = malloc(data_bytes);
    memcpy(sig16, signal, data_bytes);
    for(i=0; i<nsamples*ch; i++) {
        sig16[i] = bswap_16(sig16[i]);
    }
    md5_update(ctx, sig16, data_bytes);
    free(sig16);
#else
    md5_update(ctx, signal, data_bytes);
#endif
}

void
md5_print(uint8_t digest[16])
{
    int i;

    for(i=0; i<16; i++) {
        fprintf(stderr, "%02x", digest[i]);
    }
}
