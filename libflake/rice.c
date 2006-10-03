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
 * @file rice.c
 * Functions for calculating optimal partition order and Rice parameters
 */

#include "common.h"

#include "rice.h"

int
find_optimal_rice_param(uint32_t sum, int n)
{
    int k, k_opt;
    uint32_t nbits[MAX_RICE_PARAM+1];

    k_opt = 0;
    nbits[0] = UINT32_MAX;
    for(k=0; k<=MAX_RICE_PARAM; k++) {
        nbits[k] = rice_encode_count(sum, n, k);
        if(nbits[k] < nbits[k_opt]) {
            k_opt = k;
        }
    }
    return k_opt;
}

static uint32_t
calc_optimal_rice_params(RiceContext *rc, int porder, uint32_t *sums,
                         int n, int pred_order)
{
    int i;
    int k, cnt, part;
    uint32_t all_bits;

    part = (1 << porder);
    all_bits = 0;

    cnt = (n >> porder) - pred_order;
    for(i=0; i<part; i++) {
        if(i == 1) cnt = (n >> porder);
        k = find_optimal_rice_param(sums[i], cnt);
        rc->params[i] = k;
        all_bits += rice_encode_count(sums[i], cnt, k);
    }
    all_bits += (4 * part);

    rc->porder = porder;

    return all_bits;
}

static void
calc_sums(int pmin, int pmax, uint32_t *data, int n, int pred_order,
          uint32_t sums[][MAX_PARTITIONS])
{
    int i, j;
    int parts, cnt;
    uint32_t *res;

    // sums for highest level
    parts = (1 << pmax);
    res = &data[pred_order];
    cnt = (n >> pmax) - pred_order;
    for(i=0; i<parts; i++) {
        if(i == 1) cnt = (n >> pmax);
        if(i > 0) res = &data[i*cnt];
        sums[pmax][i] = 0;
        for(j=0; j<cnt; j++) {
            sums[pmax][i] += res[j];
        }
    }
    // sums for lower levels
    for(i=pmax-1; i>=pmin; i--) {
        parts = (1 << i);
        for(j=0; j<parts; j++) {
            sums[i][j] = sums[i+1][2*j] + sums[i+1][2*j+1];
        }
    }
}

static uint32_t
calc_rice_params(RiceContext *rc, int pmin, int pmax, int32_t *data, int n,
                 int pred_order)
{
    int i;
    uint32_t bits[MAX_PARTITION_ORDER+1];
    int opt_porder;
    RiceContext tmp_rc;
    uint32_t *udata;
    uint32_t sums[MAX_PARTITION_ORDER+1][MAX_PARTITIONS];

    assert(pmin >= 0 && pmin <= MAX_PARTITION_ORDER);
    assert(pmax >= 0 && pmax <= MAX_PARTITION_ORDER);
    assert(pmin <= pmax);

    udata = malloc(n * sizeof(uint32_t));
    for(i=0; i<n; i++) {
        udata[i] = (2*data[i]) ^ (data[i]>>31);
    }

    calc_sums(pmin, pmax, udata, n, pred_order, sums);

    opt_porder = pmin;
    bits[pmin] = UINT32_MAX;
    for(i=pmin; i<=pmax; i++) {
        bits[i] = calc_optimal_rice_params(&tmp_rc, i, sums[i], n, pred_order);
        if(bits[i] <= bits[opt_porder]) {
            opt_porder = i;
            *rc = tmp_rc;
        }
    }

    free(udata);
    return bits[opt_porder];
}

static int
get_max_p_order(int max_porder, int n, int order)
{
    int porder = MIN(max_porder, log2i(n^(n-1)));
    if(order > 0)
        porder = MIN(porder, log2i(n/order));
    return porder;
}

uint32_t
calc_rice_params_fixed(RiceContext *rc, int pmin, int pmax, int32_t *data,
                       int n, int pred_order, int bps)
{
    uint32_t bits;
    pmin = get_max_p_order(pmin, n, pred_order);
    pmax = get_max_p_order(pmax, n, pred_order);
    bits = pred_order*bps + 6;
    bits += calc_rice_params(rc, pmin, pmax, data, n, pred_order);
    return bits;
}

uint32_t
calc_rice_params_lpc(RiceContext *rc, int pmin, int pmax, int32_t *data, int n,
                     int pred_order, int bps, int precision)
{
    uint32_t bits;
    pmin = get_max_p_order(pmin, n, pred_order);
    pmax = get_max_p_order(pmax, n, pred_order);
    bits = pred_order*bps + 4 + 5 + pred_order*precision + 6;
    bits += calc_rice_params(rc, pmin, pmax, data, n, pred_order);
    return bits;
}
