/**
 * Flake: FLAC audio encoder
 * Copyright (c) 2006  Justin Ruggles <jruggle@earthlink.net>
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "optimize.h"
#include "encode.h"
#include "lpc.h"
#include "rice.h"

static void
encode_residual_verbatim(int32_t res[], int32_t smp[], int n)
{
    memcpy(res, smp, n*sizeof(int32_t));
}

static int
encode_residual_fixed(int32_t res[], int32_t smp[], int n, int order)
{
    int i;
    int64_t pred, r;

    for(i=0; i<order; i++) {
        res[i] = smp[i];
    }
    for(i=order; i<n; i++) {
        pred = 0;
        switch(order) {
            case 0: pred = 0;
                    break;
            case 1: pred = smp[i-1];
                    break;
            case 2: pred = 2*smp[i-1] - smp[i-2];
                    break;
            case 3: pred = 3*smp[i-1] - 3*smp[i-2] + smp[i-3];
                    break;
            case 4: pred = 4*smp[i-1] - 6*smp[i-2] + 4*smp[i-3] - smp[i-4];
                    break;
        }
        r = smp[i] - pred;
        if(r < INT32_MIN || r > INT32_MAX) {
            return 1;
        }
        res[i] = r;
    }
    return 0;
}

static int
encode_residual_lpc(int32_t res[], int32_t smp[], int n, int order,
                    int32_t coefs[], int shift)
{
    int i, j;
    int64_t pred, r;

    for(i=0; i<order; i++) {
        res[i] = smp[i];
    }
    for(i=order; i<n; i++) {
        pred = 0;
        for(j=0; j<order; j++) {
            pred += coefs[j] * smp[i-j-1];
        }
        r = smp[i] - (pred >> shift);
        if(r < INT32_MIN || r > INT32_MAX) {
            return 1;
        }
        res[i] = r;
    }
    return 0;
}

static inline int
get_max_p_order(int max, int n, int order) {
    int porder, max_parts;

    porder = max;
    while(porder > 0) {
        max_parts = (1 << porder);
        if(!(n % max_parts) && (n > max_parts*order)) {
            break;
        }
        porder--;
    }
    return porder;
}

int
encode_residual(FlacEncodeContext *ctx, int ch)
{
    int i;
    FlacFrame *frame;
    FlacSubframe *sub;
    int32_t coefs[MAX_LPC_ORDER][MAX_LPC_ORDER];
    int shift[MAX_LPC_ORDER];
    int n, max_order, opt_order, min_porder, max_porder;
    int32_t *res, *smp;
    int est_order, omethod;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];
    res = sub->residual;
    smp = sub->samples;
    n = frame->blocksize;

    /* CONSTANT */
    for(i=1; i<n; i++) {
        if(smp[i] != smp[0]) break;
    }
    if(i == n) {
        sub->type = sub->type_code = FLAC_SUBFRAME_CONSTANT;
        res[0] = smp[0];
        return sub->obits;
    }

    /* VERBATIM */
    if(n < 5) {
        sub->type = sub->type_code = FLAC_SUBFRAME_VERBATIM;
        encode_residual_verbatim(res, smp, n);
        return sub->obits * n;
    }

    omethod = ctx->order_method;
    max_order = ctx->max_predictor_order;
    min_porder = 0;
    max_porder = 8;

    /* FIXED */
    if(max_order == 0 || (n <= max_order)) {
        sub->order = 2;
        sub->type = FLAC_SUBFRAME_FIXED;
        sub->type_code = sub->type | sub->order;
        if(encode_residual_fixed(res, smp, n, sub->order)) {
            return -1;
        }
        return calc_rice_params_fixed(&sub->rc, min_porder, max_porder, res, n,
                                      sub->order, sub->obits);
    }

    /* LPC */
    est_order = lpc_calc_coefs(smp, n, max_order, ctx->lpc_precision,
                               ctx->order_method, coefs, shift);

    if(omethod == ORDER_METHOD_MAX) {
        // always use maximum order
        opt_order = max_order;
    } else if(omethod == ORDER_METHOD_EST) {
        // estimated order
        opt_order = est_order;
    } else if(omethod == ORDER_METHOD_2LEVEL) {
        // select the best of the estimated order or maximum order
        double bits_est, bits_max;

        i = max_order-1;
        if(encode_residual_lpc(res, smp, n, i+1, coefs[i], shift[i])) {
            bits_max = UINT32_MAX;
        } else {
            bits_max = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                            res, n, i+1, sub->obits,
                                            ctx->lpc_precision);
        }
        i = est_order-1;
        if(encode_residual_lpc(res, smp, n, i+1, coefs[i], shift[i])) {
            if(bits_max == UINT32_MAX) return -1;
            bits_est = UINT32_MAX;
        } else {
            bits_est = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                            res, n, i+1, sub->obits,
                                            ctx->lpc_precision);
        }

        opt_order = max_order;
        if(bits_est < bits_max) {
            opt_order = est_order;
        }
    } else if(omethod == ORDER_METHOD_4LEVEL) {
        uint32_t bits[4];
        int order;
        int opt_index = 3;
        opt_order = max_order-1;
        bits[3] = UINT32_MAX;
        for(i=3; i>=0; i--) {
            order = ((max_order * (i+1)) / 4)-1;
            if(order < 0) order = 0;
            if(encode_residual_lpc(res, smp, n, order+1, coefs[order],
                                   shift[order])) {
                bits[i] = UINT32_MAX;
            } else {
                bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                               res, n, order+1, sub->obits,
                                               ctx->lpc_precision);
            }
            if(bits[i] < bits[opt_index]) {
                opt_index = i;
                opt_order = order;
            }
        }
        opt_order++;
    } else if(omethod == ORDER_METHOD_8LEVEL) {
        uint32_t bits[8];
        int order;
        int opt_index = 7;
        opt_order = max_order-1;
        bits[7] = UINT32_MAX;
        for(i=7; i>=0; i--) {
            order = ((max_order * (i+1)) / 8)-1;
            if(order < 0) order = 0;
            if(encode_residual_lpc(res, smp, n, order+1, coefs[order], shift[order])) {
                bits[i] = UINT32_MAX;
            } else {
                bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                               res, n, order+1, sub->obits,
                                               ctx->lpc_precision);
            }
            if(bits[i] < bits[opt_index]) {
                opt_index = i;
                opt_order = order;
            }
        }
        opt_order++;
    } else {
        // brute-force optimal order search
        uint32_t bits[MAX_LPC_ORDER];
        opt_order = 0;
        bits[0] = UINT32_MAX;
        for(i=0; i<max_order; i++) {
            if(encode_residual_lpc(res, smp, n, i+1, coefs[i], shift[i])) {
                bits[i] = UINT32_MAX;
            } else {
                bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                               res, n, i+1, sub->obits,
                                               ctx->lpc_precision);
            }
            if(bits[i] < bits[opt_order]) {
                opt_order = i;
            }
        }
        opt_order++;
    }

    sub->order = opt_order;
    sub->type = FLAC_SUBFRAME_LPC;
    sub->type_code = sub->type | (sub->order-1);
    sub->shift = shift[sub->order-1];
    for(i=0; i<sub->order; i++) {
        sub->coefs[i] = coefs[sub->order-1][i];
    }
    if(encode_residual_lpc(res, smp, n, sub->order, sub->coefs, sub->shift)) {
        return -1;
    }
    return calc_rice_params_lpc(&sub->rc, min_porder, max_porder, res, n,
                                sub->order, sub->obits, ctx->lpc_precision);
}

void
reencode_residual_verbatim(FlacEncodeContext *ctx, int ch)
{
    FlacFrame *frame;
    FlacSubframe *sub;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];

    sub->type = sub->type_code = FLAC_SUBFRAME_VERBATIM;
    encode_residual_verbatim(sub->residual, sub->samples, frame->blocksize);
}
