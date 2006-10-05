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

#ifndef MD5_H
#define MD5_H

#include "common.h"

typedef struct {
    uint32_t lo, hi;
    uint32_t a, b, c, d;
    uint8_t buffer[64];
    uint32_t block[16];
} MD5Context;

extern void md5_init(MD5Context *ctx);

extern void md5_update(MD5Context *ctx, const void *data, uint32_t size);

extern void md5_final(uint8_t *result, MD5Context *ctx);

extern void md5_accumulate(MD5Context *ctx, const void *signal, int ch,
                           int nsamples);

extern void md5_print(uint8_t digest[16]);

#endif /* MD5_H */
