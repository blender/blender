/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bli
 * \brief Generic array manipulation API.
 */

#include "BLI_compiler_typecheck.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void _bli_array_reverse(void *arr, uint arr_len, size_t arr_stride);
#define BLI_array_reverse(arr, arr_len) _bli_array_reverse(arr, arr_len, sizeof(*(arr)))

void _bli_array_wrap(void *arr, uint arr_len, size_t arr_stride, int dir);
#define BLI_array_wrap(arr, arr_len, dir) _bli_array_wrap(arr, arr_len, sizeof(*(arr)), dir)

void _bli_array_permute(
    void *arr, const uint arr_len, const size_t arr_stride, const uint *order, void *arr_temp);
#define BLI_array_permute(arr, arr_len, order) \
  _bli_array_permute(arr, arr_len, sizeof(*(arr)), order, NULL)
#define BLI_array_permute_ex(arr, arr_len, order, arr_temp) \
  _bli_array_permute(arr, arr_len, sizeof(*(arr)), order, arr_temp)

uint _bli_array_deduplicate_ordered(void *arr, uint arr_len, size_t arr_stride);
#define BLI_array_deduplicate_ordered(arr, arr_len) \
  _bli_array_deduplicate_ordered(arr, arr_len, sizeof(*(arr)))

int _bli_array_findindex(const void *arr, uint arr_len, size_t arr_stride, const void *p);
#define BLI_array_findindex(arr, arr_len, p) _bli_array_findindex(arr, arr_len, sizeof(*(arr)), p)

int _bli_array_rfindindex(const void *arr, uint arr_len, size_t arr_stride, const void *p);
#define BLI_array_rfindindex(arr, arr_len, p) \
  _bli_array_rfindindex(arr, arr_len, sizeof(*(arr)), p)

void _bli_array_binary_and(
    void *arr, const void *arr_a, const void *arr_b, uint arr_len, size_t arr_stride);
#define BLI_array_binary_and(arr, arr_a, arr_b, arr_len) \
  (CHECK_TYPE_PAIR_INLINE(*(arr), *(arr_a)), \
   CHECK_TYPE_PAIR_INLINE(*(arr), *(arr_b)), \
   _bli_array_binary_and(arr, arr_a, arr_b, arr_len, sizeof(*(arr))))

void _bli_array_binary_or(
    void *arr, const void *arr_a, const void *arr_b, uint arr_len, size_t arr_stride);
#define BLI_array_binary_or(arr, arr_a, arr_b, arr_len) \
  (CHECK_TYPE_PAIR_INLINE(*(arr), *(arr_a)), \
   CHECK_TYPE_PAIR_INLINE(*(arr), *(arr_b)), \
   _bli_array_binary_or(arr, arr_a, arr_b, arr_len, sizeof(*(arr))))

bool _bli_array_iter_span(const void *arr,
                          uint arr_len,
                          size_t arr_stride,
                          bool use_wrap,
                          bool use_delimit_bounds,
                          bool (*test_fn)(const void *arr_item, void *user_data),
                          void *user_data,
                          uint span_step[2],
                          uint *r_span_len);
#define BLI_array_iter_span( \
    arr, arr_len, use_wrap, use_delimit_bounds, test_fn, user_data, span_step, r_span_len) \
  _bli_array_iter_span(arr, \
                       arr_len, \
                       sizeof(*(arr)), \
                       use_wrap, \
                       use_delimit_bounds, \
                       test_fn, \
                       user_data, \
                       span_step, \
                       r_span_len)

bool _bli_array_is_zeroed(const void *arr, uint arr_len, size_t arr_stride);
#define BLI_array_is_zeroed(arr, arr_len) _bli_array_is_zeroed(arr, arr_len, sizeof(*(arr)))

bool _bli_array_iter_spiral_square(const void *arr_v,
                                   const int arr_shape[2],
                                   const size_t elem_size,
                                   const int center[2],
                                   bool (*test_fn)(const void *arr_item, void *user_data),
                                   void *user_data);
#define BLI_array_iter_spiral_square(arr, arr_shape, center, test_fn, user_data) \
  _bli_array_iter_spiral_square(arr, arr_shape, sizeof(*(arr)), center, test_fn, user_data)
#ifdef __cplusplus
}
#endif
