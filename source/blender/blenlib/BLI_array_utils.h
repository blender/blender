/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/**
 * In-place array reverse.
 *
 * Access via #BLI_array_reverse
 */
void _bli_array_reverse(void *arr, uint arr_len, size_t arr_stride);
#define BLI_array_reverse(arr, arr_len) _bli_array_reverse(arr, arr_len, sizeof(*(arr)))

/**
 * In-place array wrap.
 * (rotate the array one step forward or backwards).
 *
 * Access via #BLI_array_wrap
 */
void _bli_array_wrap(void *arr, uint arr_len, size_t arr_stride, int dir);
#define BLI_array_wrap(arr, arr_len, dir) _bli_array_wrap(arr, arr_len, sizeof(*(arr)), dir)

/**
 *In-place array permute.
 * (re-arrange elements based on an array of indices).
 *
 * Access via #BLI_array_wrap
 */
void _bli_array_permute(
    void *arr, uint arr_len, size_t arr_stride, const uint *order, void *arr_temp);
#define BLI_array_permute(arr, arr_len, order) \
  _bli_array_permute(arr, arr_len, sizeof(*(arr)), order, NULL)
#define BLI_array_permute_ex(arr, arr_len, order, arr_temp) \
  _bli_array_permute(arr, arr_len, sizeof(*(arr)), order, arr_temp)

/**
 * In-place array de-duplication of an ordered array.
 *
 * \return The new length of the array.
 *
 * Access via #BLI_array_deduplicate_ordered
 */
uint _bli_array_deduplicate_ordered(void *arr, uint arr_len, size_t arr_stride);
#define BLI_array_deduplicate_ordered(arr, arr_len) \
  _bli_array_deduplicate_ordered(arr, arr_len, sizeof(*(arr)))

/**
 * Find the first index of an item in an array.
 *
 * Access via #BLI_array_findindex
 *
 * \note Not efficient, use for error checks/asserts.
 */
int _bli_array_findindex(const void *arr, uint arr_len, size_t arr_stride, const void *p);
#define BLI_array_findindex(arr, arr_len, p) _bli_array_findindex(arr, arr_len, sizeof(*(arr)), p)

/**
 * A version of #BLI_array_findindex that searches from the end of the list.
 */
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

/**
 * Utility function to iterate over contiguous items in an array.
 *
 * \param use_wrap: Detect contiguous ranges across the first/last points.
 * In this case the second index of \a span_step may be lower than the first,
 * which indicates the values are wrapped.
 * \param use_delimit_bounds: When false,
 * ranges that defined by the start/end indices are excluded.
 * This option has no effect when \a use_wrap is enabled.
 * \param test_fn: Function to test if the item should be included in the range.
 * \param user_data: User data for \a test_fn.
 * \param span_step: Indices to iterate over,
 * initialize both values to the array length to initialize iteration.
 * \param r_span_len: The length of the span, useful when \a use_wrap is enabled,
 * where calculating the length isn't a simple subtraction.
 */
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

/**
 * Simple utility to check memory is zeroed.
 */
bool _bli_array_is_zeroed(const void *arr, uint arr_len, size_t arr_stride);
#define BLI_array_is_zeroed(arr, arr_len) _bli_array_is_zeroed(arr, arr_len, sizeof(*(arr)))

/**
 * Smart function to sample a rectangle spiraling outside.
 * Nice for selection ID.
 *
 * \param arr_shape: dimensions [w, h].
 * \param center: coordinates [x, y] indicating where to start traversing.
 */
bool _bli_array_iter_spiral_square(const void *arr_v,
                                   const int arr_shape[2],
                                   size_t elem_size,
                                   const int center[2],
                                   bool (*test_fn)(const void *arr_item, void *user_data),
                                   void *user_data);
#define BLI_array_iter_spiral_square(arr, arr_shape, center, test_fn, user_data) \
  _bli_array_iter_spiral_square(arr, arr_shape, sizeof(*(arr)), center, test_fn, user_data)
#ifdef __cplusplus
}
#endif
