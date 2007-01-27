/**
 * Flake: FLAC audio encoder
 * Copyright (c) 2006  Justin Ruggles
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"

#include "crc.h"

static void
crc_init_table(uint16_t *table, int bits, int poly)
{
    int i, j, crc;

    poly = (poly + (1<<bits));
    for(i=0; i<256; i++) {
        crc = i;
        for(j=0; j<bits; j++) {
            if(crc & (1<<(bits-1))) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
        table[i] = (crc & ((1<<bits)-1));
    }
}

/* CRC key for polynomial, x^8 + x^2 + x^1 + 1 */
#define CRC8_POLY 0x07

/* CRC key for polynomial, x^16 + x^15 + x^2 + 1 */
#define CRC16_POLY 0x8005

static uint16_t crc8tab[256];
static uint16_t crc16tab[256];

void
crc_init()
{
    crc_init_table(crc8tab, 8, CRC8_POLY);
    crc_init_table(crc16tab, 16, CRC16_POLY);
}

static uint16_t
calc_crc(const uint16_t *table, int bits, const uint8_t *data, uint32_t len)
{
	uint16_t crc, v1, v2;

    crc = 0;
    while(len--) {
        v1 = (crc << 8) & ((1 << bits) - 1);
        v2 = (crc >> (bits - 8)) ^ *data++;
        assert(v2 < 256);
        crc = v1 ^ table[v2];
    }
    return crc;
}

uint8_t
calc_crc8(const uint8_t *data, uint32_t len)
{
	uint8_t crc;

    if(data == NULL) return 0;

    crc = calc_crc(crc8tab, 8, data, len);
    return crc;
}

uint16_t
calc_crc16(const uint8_t *data, uint32_t len)
{
	uint16_t crc;

    if(data == NULL) return 0;

    crc = calc_crc(crc16tab, 16, data, len);
    return crc;
}
