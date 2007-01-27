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
#include "lpc.h"

/**
 * Apply Welch window function to audio block
 */
static inline void
apply_welch_window(const int32_t *data, int len, double *w_data)
{
    int i;
    double c;

    c = (2.0 / (len - 1.0)) - 1.0;
    for(i=0; i<(len >> 1); i++) {
        double w = 1.0 - ((c-i) * (c-i));
        w_data[i] = data[i] * w;
        w_data[len-1-i] = data[len-1-i] * w;
    }
}

/**
 * Calculates autocorrelation data from audio samples
 * A Welch window function is applied before calculation.
 */
static void
compute_autocorr(const int32_t *data, int len, int lag, double *autoc)
{
    int i, j;
    double *data1;
    double temp, temp2;

    data1 = malloc((len+16) * sizeof(double));
    apply_welch_window(data, len, data1);
    data1[len] = 0;

    for (i=0; i<=lag; ++i) {
        temp = 1.0;
        temp2 = 1.0;
        for (j=0; j<=lag-i; ++j)
            temp += data1[j+i] * data1[j];

        for (j=lag+1; j<=len-1; j+=2) {
            temp += data1[j] * data1[j-i];
            temp2 += data1[j+1] * data1[j+1-i];
        }
        autoc[i] = temp + temp2;
    }

    free(data1);
}

/**
 * Levinson-Durbin recursion.
 * Produces LPC coefficients from autocorrelation data.
 */
static void
compute_lpc_coefs(const double *autoc, int max_order, double *ref,
                  double lpc[][MAX_LPC_ORDER])
{
    int i, j, i2;
    double r, err, tmp;
    double lpc_tmp[MAX_LPC_ORDER];

    for(i=0; i<max_order; i++) lpc_tmp[i] = 0;
    err = 1.0;
    if(autoc) {
        err = autoc[0];
    }

    for(i=0; i<max_order; i++) {
        if(ref) {
            r = ref[i];
        } else {
            r = -autoc[i+1];
            for(j=0; j<i; j++) {
                r -= lpc_tmp[j] * autoc[i-j];
            }
            r /= err;
            err *= 1.0 - (r * r);
        }

        i2 = (i >> 1);
        lpc_tmp[i] = r;
        for(j=0; j<i2; j++) {
            tmp = lpc_tmp[j];
            lpc_tmp[j] += r * lpc_tmp[i-1-j];
            lpc_tmp[i-1-j] += r * tmp;
        }
        if(i & 1) {
            lpc_tmp[j] += lpc_tmp[j] * r;
        }

        for(j=0; j<=i; j++) {
            lpc[i][j] = -lpc_tmp[j];
        }
    }
}

/**
 * Compute LPC coefs for FLAKE_ORDER_METHOD_EST
 * Faster LPC coeff computation by first calculating the reflection coefficients
 * using Schur recursion. That allows for estimating the optimal order before
 * running Levinson recursion.
 */
static int
compute_lpc_coefs_est(const double *autoc, int max_order,
                      double lpc[][MAX_LPC_ORDER])
{
    int i, j;
    double error;
    double gen[2][MAX_LPC_ORDER];
    double ref[MAX_LPC_ORDER];
    int order_est;

    // Schur recursion
    for(i=0; i<max_order; i++) gen[0][i] = gen[1][i] = autoc[i+1];
    error = autoc[0];
    ref[0] = -gen[1][0] / error;
    error += gen[1][0] * ref[0];
    for(i=1; i<max_order; i++) {
        for(j=0; j<max_order-i; j++) {
            gen[1][j] = gen[1][j+1] + ref[i-1] * gen[0][j];
            gen[0][j] = gen[1][j+1] * ref[i-1] + gen[0][j];
        }
        ref[i] = -gen[1][0] / error;
        error += gen[1][0] * ref[i];
    }

    // Estimate optimal order using reflection coefficients
    order_est = 1;
    for(i=max_order-1; i>=0; i--) {
        if(fabs(ref[i]) > 0.10) {
            order_est = i+1;
            break;
        }
    }

    // Levinson recursion
    compute_lpc_coefs(NULL, order_est, ref, lpc);

    return order_est;
}

/**
 * Quantize LPC coefficients
 */
static void
quantize_lpc_coefs(double *lpc_in, int order, int precision, int32_t *lpc_out,
                   int *shift)
{
    int i;
    double d, cmax, error;
    int32_t qmax;
    int sh, q;

    // define maximum levels
    qmax = (1 << (precision - 1)) - 1;

    // find maximum coefficient value
    cmax = 0.0;
    for(i=0; i<order; i++) {
        d = fabs(lpc_in[i]);
        if(d > cmax)
            cmax = d;
    }
    // if maximum value quantizes to zero, return all zeros
    if(cmax * (1 << 15) < 1.0) {
        *shift = 0;
        memset(lpc_out, 0, sizeof(int32_t) * order);
        return;
    }

    // calculate level shift which scales max coeff to available bits
    sh = 15;
    while((cmax * (1 << sh) > qmax) && (sh > 0)) {
        sh--;
    }

    // since negative shift values are unsupported in decoder, scale down
    // coefficients instead
    if(sh == 0 && cmax > qmax) {
        double scale = ((double)qmax) / cmax;
        for(i=0; i<order; i++) {
            lpc_in[i] *= scale;
        }
    }

    // output quantized coefficients and level shift
    error=0;
    for(i=0; i<order; i++) {
        error += lpc_in[i] * (1 << sh);
        q = error + 0.5;
        if(q <= -qmax) q = -qmax+1;
        if(q > qmax) q = qmax;
        error -= q;
        lpc_out[i] = q;
    }
    *shift = sh;
}

/**
 * Calculate LPC coefficients for multiple orders
 */
int
lpc_calc_coefs(const int32_t *samples, int blocksize, int max_order,
               int precision, int omethod, int32_t coefs[][MAX_LPC_ORDER],
               int *shift)
{
    double autoc[MAX_LPC_ORDER+1];
    double lpc[MAX_LPC_ORDER][MAX_LPC_ORDER];
    int i;
    int opt_order;

    compute_autocorr(samples, blocksize, max_order+1, autoc);

    opt_order = max_order;
    if(omethod == FLAKE_ORDER_METHOD_EST) {
        opt_order = compute_lpc_coefs_est(autoc, max_order, lpc);
    } else {
        compute_lpc_coefs(autoc, max_order, NULL, lpc);
    }

    switch(omethod) {
        case FLAKE_ORDER_METHOD_MAX:
        case FLAKE_ORDER_METHOD_EST:
            i = opt_order-1;
            quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
            break;
        default:
            for(i=0; i<max_order; i++) {
                quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
            }
            break;
    }

    return opt_order;
}
