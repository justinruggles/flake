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

#include "flake.h"
#include "lpc.h"

/**
 * Apply Welch window function to audio block
 */
static void
apply_welch_window(const int32_t *data, int len, double *w_data)
{
    int i, n2;
    double w;
    double c;

    n2 = (len >> 1);
    c = 2.0 / (len - 1.0);
	for(i=0; i<n2; i++) {
		w = c - i - 1.0;
        w = 1.0 - (w * w);
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

    data1 = calloc((len+1), sizeof(double));
    apply_welch_window(data, len, data1);

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
compute_lpc_coefs(const double *autoc, int max_order,
                  double lpc[][MAX_LPC_ORDER], double *ref)
{
   int i, j, i2;
   double r, err, tmp;
   double lpc_tmp[MAX_LPC_ORDER];

   for(i=0; i<max_order; i++) lpc_tmp[i] = 0;
   err = autoc[0];

   for(i=0; i<max_order; i++) {
      r = -autoc[i+1];
      for(j=0; j<i; j++) {
          r -= lpc_tmp[j] * autoc[i-j];
      }
      r /= err;
      ref[i] = fabs(r);

      err *= 1.0 - (r * r);

      i2 = (i >> 1);
      lpc_tmp[i] = r;
      for(j=0; j<i2; j++) {
         tmp = lpc_tmp[j];
         lpc_tmp[j] += r * lpc_tmp[i-1-j];
         lpc_tmp[i-1-j] += r * tmp;
      }
      if(i % 2) {
          lpc_tmp[j] += lpc_tmp[j] * r;
      }

      for(j=0; j<=i; j++) {
          lpc[i][j] = -lpc_tmp[j];
      }
   }
}

/**
 * Quantize LPC coefficients
 */
static void
quantize_lpc_coefs(double *lpc_in, int order, int precision, int32_t *lpc_out,
                   int *shift)
{
	int i;
	double d, cmax;
	int32_t qmax;
	int sh, max_shift;

    /* limit order & precision to FLAC specification */
    assert(order >= 0 && order <= MAX_LPC_ORDER);
	assert(precision > 0 && precision < 16);

    /* define maximum levels */
    max_shift = 15;
    qmax = (1 << (precision - 1)) - 1;

    /* find maximum coefficient value */
    cmax = 0.0;
    for(i=0; i<order; i++) {
        d = lpc_in[i];
        if(d < 0) d = -d;
        if(d > cmax)
            cmax = d;
    }

    /* if maximum value quantizes to zero, return all zeros */
    if(cmax * (1 << max_shift) < 1.0) {
        *shift = 0;
        for(i=0; i<order; i++) {
            lpc_out[i] = 0;
        }
        return;
    }

    /* calculate level shift which scales max coeff to available bits */
    sh = max_shift;
    while((cmax * (1 << sh) > qmax) && (sh > 0)) {
        sh--;
    }

    /* since negative shift values are unsupported in decoder, scale down
       coefficients instead */
    if(sh == 0 && cmax > qmax) {
        double scale = ((double)qmax) / cmax;
        for(i=0; i<order; i++) {
            lpc_in[i] *= scale;
        }
    }

    /* output quantized coefficients and level shift */
    for(i=0; i<order; i++) {
        lpc_out[i] = (int32_t)(lpc_in[i] * (1 << sh));
    }
    *shift = sh;
}

static int
estimate_best_order(double *ref, int max_order)
{
    int i, est;

    est = 1;
    for(i=max_order-1; i>=0; i--) {
        if(ref[i] > 0.10) {
            est = i+1;
            break;
        }
    }
    return est;
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
    double ref[MAX_LPC_ORDER];
    double lpc[MAX_LPC_ORDER][MAX_LPC_ORDER];
    int i, j;
    int opt_order;

    /* order 0 is not valid in LPC mode */
    if(max_order < 1) return 1;

    compute_autocorr(samples, blocksize, max_order+1, autoc);

    compute_lpc_coefs(autoc, max_order, lpc, ref);

    opt_order = max_order;
    if(omethod == FLAKE_ORDER_METHOD_EST || omethod == FLAKE_ORDER_METHOD_SEARCH) {
        opt_order = estimate_best_order(ref, max_order);
    }
    switch(omethod) {
        case FLAKE_ORDER_METHOD_MAX:
        case FLAKE_ORDER_METHOD_EST:
            i = opt_order-1;
            quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
            break;
        case FLAKE_ORDER_METHOD_2LEVEL:
            i = (max_order/2)-1;
            if(i < 0) i = 0;
            quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
            i = max_order-1;
            quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
            break;
        case FLAKE_ORDER_METHOD_4LEVEL:
            for(j=1; j<=4; j++) {
                i = (max_order*j/4)-1;
                if(i < 0) i = 0;
                quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
            }
            break;
        case FLAKE_ORDER_METHOD_8LEVEL:
            for(j=1; j<=8; j++) {
                i = (max_order*j/8)-1;
                if(i < 0) i = 0;
                quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
            }
            break;
        case FLAKE_ORDER_METHOD_SEARCH:
            for(i=0; i<max_order; i++) {
                quantize_lpc_coefs(lpc[i], i+1, precision, coefs[i], &shift[i]);
            }
            break;
    }

    return opt_order;
}


