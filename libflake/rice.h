/**
 * Flake: FLAC audio encoder
 * Copyright (c) 2006 Justin Ruggles
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

/**
 * @file rice.h
 * Functions for calculating optimal partition order and Rice parameters
 */

#ifndef RICE_H
#define RICE_H

#include "common.h"

#define MAX_RICE_PARAM          14
#define MAX_PARTITION_ORDER     8
#define MAX_PARTITIONS          (1 << MAX_PARTITION_ORDER)

typedef struct RiceContext {
    int porder;                     /* partition order */
    int params[MAX_PARTITIONS];     /* Rice parameters */
    int esc_bps[MAX_PARTITIONS];    /* bps if using escape code */
} RiceContext;

#define rice_encode_count(sum, n, k) (((n)*((k)+1))+(((sum)-(n>>1))>>(k)))

extern int find_optimal_rice_param(uint32_t sum, int n);

extern uint32_t calc_rice_params_fixed(RiceContext *rc, int pmin, int pmax,
                                       int32_t *data, int n, int pred_order,
                                       int bps);

extern uint32_t calc_rice_params_lpc(RiceContext *rc, int pmin, int pmax,
                                     int32_t *data, int n, int pred_order,
                                     int bps, int precision);

#endif /* RICE_H */
