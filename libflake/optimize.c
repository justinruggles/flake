/**
 * Flake: FLAC audio encoder
 * Copyright (c) 2006-2007 Justin Ruggles
 *
 * Flake is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Flake is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Flake; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "common.h"

#include "flake.h"
#include "optimize.h"
#include "encode.h"
#include "lpc.h"
#include "rice.h"

static void
encode_residual_verbatim(int32_t *res, int32_t *smp, int n)
{
    memcpy(res, smp, n*sizeof(int32_t));
}

static void
encode_residual_fixed(int32_t *res, int32_t *smp, int n, int order)
{
    int i;

    if(order) {
        memcpy(res, smp, order*sizeof(int32_t));
    } else {
        memcpy(res, smp, n*sizeof(int32_t));
        return;
    }
    switch(order) {
        case 1:
            for(i=1; i<n; i++) {
                res[i] = (int32_t)(smp[i] - smp[i-1]);
            }
            return;
        case 2:
            for(i=2; i<n; i++) {
                res[i] = (int32_t)(smp[i] - 2LL*smp[i-1] + smp[i-2]);
            }
            return;
        case 3:
            for(i=3; i<n; i++) {
                res[i] = (int32_t)(smp[i] - 3LL*smp[i-1] + 3LL*smp[i-2] - smp[i-3]);
            }
            return;
        case 4:
            for(i=4; i<n; i++) {
                res[i] = (int32_t)(smp[i] - 4LL*smp[i-1] + 6LL*smp[i-2] - 4LL*smp[i-3] + smp[i-4]);
            }
            return;
        default: return;
    }
}

static void
encode_residual_lpc(int32_t *res, int32_t *smp, int n, int order,
                    int32_t *coefs, int shift)
{
    int i;
    int64_t pred;

    for(i=0; i<order; i++) {
        res[i] = smp[i];
    }
    for(i=order; i<n; i++) {
        pred = 0;
        // note that all cases fall through.
        // the result is in an unrolled loop for each order
        switch(order) {
            case 32: pred += (int64_t)coefs[31] * (int64_t)smp[i-32];
            case 31: pred += (int64_t)coefs[30] * (int64_t)smp[i-31];
            case 30: pred += (int64_t)coefs[29] * (int64_t)smp[i-30];
            case 29: pred += (int64_t)coefs[28] * (int64_t)smp[i-29];
            case 28: pred += (int64_t)coefs[27] * (int64_t)smp[i-28];
            case 27: pred += (int64_t)coefs[26] * (int64_t)smp[i-27];
            case 26: pred += (int64_t)coefs[25] * (int64_t)smp[i-26];
            case 25: pred += (int64_t)coefs[24] * (int64_t)smp[i-25];
            case 24: pred += (int64_t)coefs[23] * (int64_t)smp[i-24];
            case 23: pred += (int64_t)coefs[22] * (int64_t)smp[i-23];
            case 22: pred += (int64_t)coefs[21] * (int64_t)smp[i-22];
            case 21: pred += (int64_t)coefs[20] * (int64_t)smp[i-21];
            case 20: pred += (int64_t)coefs[19] * (int64_t)smp[i-20];
            case 19: pred += (int64_t)coefs[18] * (int64_t)smp[i-19];
            case 18: pred += (int64_t)coefs[17] * (int64_t)smp[i-18];
            case 17: pred += (int64_t)coefs[16] * (int64_t)smp[i-17];
            case 16: pred += (int64_t)coefs[15] * (int64_t)smp[i-16];
            case 15: pred += (int64_t)coefs[14] * (int64_t)smp[i-15];
            case 14: pred += (int64_t)coefs[13] * (int64_t)smp[i-14];
            case 13: pred += (int64_t)coefs[12] * (int64_t)smp[i-13];
            case 12: pred += (int64_t)coefs[11] * (int64_t)smp[i-12];
            case 11: pred += (int64_t)coefs[10] * (int64_t)smp[i-11];
            case 10: pred += (int64_t)coefs[ 9] * (int64_t)smp[i-10];
            case  9: pred += (int64_t)coefs[ 8] * (int64_t)smp[i- 9];
            case  8: pred += (int64_t)coefs[ 7] * (int64_t)smp[i- 8];
            case  7: pred += (int64_t)coefs[ 6] * (int64_t)smp[i- 7];
            case  6: pred += (int64_t)coefs[ 5] * (int64_t)smp[i- 6];
            case  5: pred += (int64_t)coefs[ 4] * (int64_t)smp[i- 5];
            case  4: pred += (int64_t)coefs[ 3] * (int64_t)smp[i- 4];
            case  3: pred += (int64_t)coefs[ 2] * (int64_t)smp[i- 3];
            case  2: pred += (int64_t)coefs[ 1] * (int64_t)smp[i- 2];
            case  1: pred += (int64_t)coefs[ 0] * (int64_t)smp[i- 1];
            case  0:
            default: break;
        }
        res[i] = (int32_t)((int64_t)smp[i] - (pred >> shift));
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
