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

#include "flake.h"
#include "optimize.h"
#include "encode.h"
#include "lpc.h"
#include "rice.h"

static void
encode_residual_verbatim(int32_t res[], int32_t smp[], int n)
{
    memcpy(res, smp, n*sizeof(int32_t));
}

static void
encode_residual_fixed(int32_t res[], int32_t smp[], int n, int order)
{
    int i;

    switch(order) {
        case 0:
            memcpy(res, smp, n*sizeof(int32_t));
            return;
        case 1:
            res[0] = smp[0];
            for(i=1; i<n; i++) {
                res[i] = smp[i] - (smp[i-1]);
            }
            return;
        case 2:
            res[0] = smp[0];
            res[1] = smp[1];
            for(i=2; i<n; i++) {
                res[i] = smp[i] - 2*smp[i-1] + smp[i-2];
            }
            return;
        case 3:
            res[0] = smp[0];
            res[1] = smp[1];
            res[2] = smp[2];
            for(i=3; i<n; i++) {
                res[i] = smp[i] - 3*smp[i-1] + 3*smp[i-2] - smp[i-3];
            }
            return;
        case 4:
            res[0] = smp[0];
            res[1] = smp[1];
            res[2] = smp[2];
            res[3] = smp[3];
            for(i=4; i<n; i++) {
                res[i] = smp[i] - 4*smp[i-1] + 6*smp[i-2] - 4*smp[i-3] + smp[i-4];
            }
            return;
        default: return;
    }
}

static void
encode_residual_lpc(int32_t res[], int32_t smp[], int n, int order,
                    int32_t coefs[], int shift)
{
    int i;
    int32_t pred;

    for(i=0; i<order; i++) {
        res[i] = smp[i];
    }
    for(i=order; i<n; i++) {
        pred = 0;
        // note that all cases fall through.
        // the result is in an unrolled loop for each order
        switch(order) {
            case 32: pred += coefs[31] * smp[i-32];
            case 31: pred += coefs[30] * smp[i-31];
            case 30: pred += coefs[29] * smp[i-30];
            case 29: pred += coefs[28] * smp[i-29];
            case 28: pred += coefs[27] * smp[i-28];
            case 27: pred += coefs[26] * smp[i-27];
            case 26: pred += coefs[25] * smp[i-26];
            case 25: pred += coefs[24] * smp[i-25];
            case 24: pred += coefs[23] * smp[i-24];
            case 23: pred += coefs[22] * smp[i-23];
            case 22: pred += coefs[21] * smp[i-22];
            case 21: pred += coefs[20] * smp[i-21];
            case 20: pred += coefs[19] * smp[i-20];
            case 19: pred += coefs[18] * smp[i-19];
            case 18: pred += coefs[17] * smp[i-18];
            case 17: pred += coefs[16] * smp[i-17];
            case 16: pred += coefs[15] * smp[i-16];
            case 15: pred += coefs[14] * smp[i-15];
            case 14: pred += coefs[13] * smp[i-14];
            case 13: pred += coefs[12] * smp[i-13];
            case 12: pred += coefs[11] * smp[i-12];
            case 11: pred += coefs[10] * smp[i-11];
            case 10: pred += coefs[ 9] * smp[i-10];
            case  9: pred += coefs[ 8] * smp[i- 9];
            case  8: pred += coefs[ 7] * smp[i- 8];
            case  7: pred += coefs[ 6] * smp[i- 7];
            case  6: pred += coefs[ 5] * smp[i- 6];
            case  5: pred += coefs[ 4] * smp[i- 5];
            case  4: pred += coefs[ 3] * smp[i- 4];
            case  3: pred += coefs[ 2] * smp[i- 3];
            case  2: pred += coefs[ 1] * smp[i- 2];
            case  1: pred += coefs[ 0] * smp[i- 1];
            case  0:
            default: break;
        }
        res[i] = smp[i] - (pred >> shift);
    }
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
    int min_order;
    int32_t *res, *smp;
    int est_order, omethod;

    frame = &ctx->frame;
    sub = &frame->subframes[ch];
    res = sub->residual;
    smp = sub->samples;
    n = frame->blocksize;

    // CONSTANT
    for(i=1; i<n; i++) {
        if(smp[i] != smp[0]) break;
    }
    if(i == n) {
        sub->type = sub->type_code = FLAC_SUBFRAME_CONSTANT;
        res[0] = smp[0];
        return sub->obits;
    }

    // VERBATIM
    if(n < 5 || ctx->params.prediction_type == FLAKE_PREDICTION_NONE) {
        sub->type = sub->type_code = FLAC_SUBFRAME_VERBATIM;
        encode_residual_verbatim(res, smp, n);
        return sub->obits * n;
    }

    omethod = ctx->params.order_method;
    min_order = ctx->params.min_prediction_order;
    max_order = ctx->params.max_prediction_order;
    opt_order = max_order;
    min_porder = ctx->params.min_partition_order;
    max_porder = ctx->params.max_partition_order;

    // FIXED
    if(ctx->params.prediction_type == FLAKE_PREDICTION_FIXED || n <= max_order) {
        uint32_t bits[5];
        if(max_order > 4) max_order = 4;
        opt_order = min_order;
        bits[opt_order] = UINT32_MAX;
        for(i=min_order; i<=max_order; i++) {
            encode_residual_fixed(res, smp, n, i);
            bits[i] = calc_rice_params_fixed(&sub->rc, min_porder, max_porder, res,
                                             n, i, sub->obits);
            if(bits[i] < bits[opt_order]) {
                opt_order = i;
            }
        }
        sub->order = opt_order;
        sub->type = FLAC_SUBFRAME_FIXED;
        sub->type_code = sub->type | sub->order;
        if(sub->order != max_order) {
            encode_residual_fixed(res, smp, n, sub->order);
            return calc_rice_params_fixed(&sub->rc, min_porder, max_porder, res, n,
                                          sub->order, sub->obits);
        }
        return bits[sub->order];
    }

    // LPC
    est_order = lpc_calc_coefs(smp, n, max_order, ctx->lpc_precision,
                               omethod, coefs, shift);

    if(omethod == FLAKE_ORDER_METHOD_MAX) {
        // always use maximum order
        opt_order = max_order;
    } else if(omethod == FLAKE_ORDER_METHOD_EST) {
        // estimated order
        opt_order = est_order;
    } else if(omethod == FLAKE_ORDER_METHOD_2LEVEL ||
              omethod == FLAKE_ORDER_METHOD_4LEVEL ||
              omethod == FLAKE_ORDER_METHOD_8LEVEL) {
        int levels = 1 << (omethod-1);
        uint32_t bits[8];
        int order;
        int opt_index = levels-1;
        opt_order = max_order-1;
        bits[opt_index] = UINT32_MAX;
        for(i=opt_index; i>=0; i--) {
            order = min_order + (((max_order-min_order+1) * (i+1)) / levels)-1;
            if(order < 0) order = 0;
            encode_residual_lpc(res, smp, n, order+1, coefs[order], shift[order]);
            bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                           res, n, order+1, sub->obits,
                                           ctx->lpc_precision);
            if(bits[i] < bits[opt_index]) {
                opt_index = i;
                opt_order = order;
            }
        }
        opt_order++;
    } else if(omethod == FLAKE_ORDER_METHOD_SEARCH) {
        // brute-force optimal order search
        uint32_t bits[MAX_LPC_ORDER];
        opt_order = 0;
        bits[0] = UINT32_MAX;
        for(i=0; i<max_order; i++) {
            encode_residual_lpc(res, smp, n, i+1, coefs[i], shift[i]);
            bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                           res, n, i+1, sub->obits,
                                           ctx->lpc_precision);
            if(bits[i] < bits[opt_order]) {
                opt_order = i;
            }
        }
        opt_order++;
    } else if(omethod == FLAKE_ORDER_METHOD_LOG) {
        // log search (written by Michael Niedermayer for FFmpeg)
        uint32_t bits[MAX_LPC_ORDER];
        int step;

        opt_order = min_order - 1 + (max_order-min_order)/3;
        memset(bits, -1, sizeof(bits));

        for(step=16; step>0; step>>=1){
            int last = opt_order;
            for(i=last-step; i<=last+step; i+= step){
                if(i<min_order-1 || i>=max_order || bits[i] < UINT32_MAX)
                    continue;
                encode_residual_lpc(res, smp, n, i+1, coefs[i], shift[i]);
                bits[i] = calc_rice_params_lpc(&sub->rc, min_porder, max_porder,
                                               res, n, i+1, sub->obits,
                                               ctx->lpc_precision);
                if(bits[i] < bits[opt_order]) {
                    opt_order = i;
                }
            }
        }
        opt_order++;
    } else {
        return -1;
    }

    sub->order = opt_order;
    sub->type = FLAC_SUBFRAME_LPC;
    sub->type_code = sub->type | (sub->order-1);
    sub->shift = shift[sub->order-1];
    for(i=0; i<sub->order; i++) {
        sub->coefs[i] = coefs[sub->order-1][i];
    }
    encode_residual_lpc(res, smp, n, sub->order, sub->coefs, sub->shift);
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
