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

#ifndef LPC_H
#define LPC_H

#include "config.h"

#include <stdlib.h>
#include <inttypes.h>

#define MAX_LPC_ORDER 32

#define ORDER_METHOD_MAX    0
#define ORDER_METHOD_EST    1
#define ORDER_METHOD_2LEVEL 2
#define ORDER_METHOD_4LEVEL 3
#define ORDER_METHOD_8LEVEL 4
#define ORDER_METHOD_SEARCH 5

extern int lpc_calc_coefs(const int32_t *samples, int blocksize, int max_order,
                          int precision, int omethod,
                          int32_t coefs[][MAX_LPC_ORDER], int *shift);

#endif /* LPC_H */
