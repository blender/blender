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

/** \file
 * \ingroup bli
 * \brief Generic array manipulation API.
 *
 * \warning Some array operations here are inherently inefficient,
 * and only included for the cases where the performance is acceptable.
 * Use with care.
 */
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math_base.h"
#include "BLI_strict_flags.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "BLI_array_utils.h"

/**
 *In-place array reverse.
 *
 * Access via #BLI_array_reverse
 */
void _bli_array_reverse(void *arr_v, unsigned int arr_len, size_t arr_stride)
{
  const unsigned int arr_stride_uint = (unsigned int)arr_stride;
  const unsigned int arr_half_stride = (arr_len / 2) * arr_stride_uint;
  unsigned int i, i_end;
  char *arr = arr_v;
  char *buf = BLI_array_alloca(buf, arr_stride);

  for (i = 0, i_end = (arr_len - 1) * arr_stride_uint; i < arr_half_stride;
       i += arr_stride_uint, i_end -= arr_stride_uint) {
    memcpy(buf, &arr[i], arr_stride);
    memcpy(&arr[i], &arr[i_end], arr_stride);
    memcpy(&arr[i_end], buf, arr_stride);
  }
}

/**
 * In-place array wrap.
 * (rotate the array one step forward or backwards).
 *
 * Access via #BLI_array_wrap
 */
void _bli_array_wrap(void *arr_v, unsigned int arr_len, size_t arr_stride, int dir)
{
  char *arr = arr_v;
  char *buf = BLI_array_alloca(buf, arr_stride);

  if (dir == -1) {
    memcpy(buf, arr, arr_stride);
    memmove(arr, arr + arr_stride, arr_stride * (arr_len - 1));
    memcpy(arr + (arr_stride * (arr_len - 1)), buf, arr_stride);
  }
  else if (dir == 1) {
    memcpy(buf, arr + (arr_stride * (arr_len - 1)), arr_stride);
    memmove(arr + arr_stride, arr, arr_stride * (arr_len - 1));
    memcpy(arr, buf, arr_stride);
  }
  else {
    BLI_assert(0);
  }
}

/**
 *In-place array permute.
 * (re-arrange elements based on an array of indices).
 *
 * Access via #BLI_array_wrap
 */
void _bli_array_permute(void *arr,
                        const unsigned int arr_len,
                        const size_t arr_stride,
                        const unsigned int *order,
                        void *arr_temp)
{
  const size_t len = arr_len * arr_stride;
  const unsigned int arr_stride_uint = (unsigned int)arr_stride;
  void *arr_orig;
  unsigned int i;

  if (arr_temp == NULL) {
    arr_orig = MEM_mallocN(len, __func__);
  }
  else {
    arr_orig = arr_temp;
  }

  memcpy(arr_orig, arr, len);

  for (i = 0; i < arr_len; i++) {
    BLI_assert(order[i] < arr_len);
    memcpy(POINTER_OFFSET(arr, arr_stride_uint * i),
           POINTER_OFFSET(arr_orig, arr_stride_uint * order[i]),
           arr_stride);
  }

  if (arr_temp == NULL) {
    MEM_freeN(arr_orig);
  }
}

/**
 * Find the first index of an item in an array.
 *
 * Access via #BLI_array_findindex
 *
 * \note Not efficient, use for error checks/asserts.
 */
int _bli_array_findindex(const void *arr, unsigned int arr_len, size_t arr_stride, const void *p)
{
  const char *arr_step = (const char *)arr;
  for (unsigned int i = 0; i < arr_len; i++, arr_step += arr_stride) {
    if (memcmp(arr_step, p, arr_stride) == 0) {
      return (int)i;
    }
  }
  return -1;
}

/**
 * A version of #BLI_array_findindex that searches from the end of the list.
 */
int _bli_array_rfindindex(const void *arr, unsigned int arr_len, size_t arr_stride, const void *p)
{
  const char *arr_step = (const char *)arr + (arr_stride * arr_len);
  for (unsigned int i = arr_len; i-- != 0;) {
    arr_step -= arr_stride;
    if (memcmp(arr_step, p, arr_stride) == 0) {
      return (int)i;
    }
  }
  return -1;
}

void _bli_array_binary_and(
    void *arr, const void *arr_a, const void *arr_b, unsigned int arr_len, size_t arr_stride)
{
  char *dst = arr;
  const char *src_a = arr_a;
  const char *src_b = arr_b;

  size_t i = arr_stride * arr_len;
  while (i--) {
    *(dst++) = *(src_a++) & *(src_b++);
  }
}

void _bli_array_binary_or(
    void *arr, const void *arr_a, const void *arr_b, unsigned int arr_len, size_t arr_stride)
{
  char *dst = arr;
  const char *src_a = arr_a;
  const char *src_b = arr_b;

  size_t i = arr_stride * arr_len;
  while (i--) {
    *(dst++) = *(src_a++) | *(src_b++);
  }
}

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
 * where calculating the length isnt a simple subtraction.
 */
bool _bli_array_iter_span(const void *arr,
                          unsigned int arr_len,
                          size_t arr_stride,
                          bool use_wrap,
                          bool use_delimit_bounds,
                          bool (*test_fn)(const void *arr_item, void *user_data),
                          void *user_data,
                          unsigned int span_step[2],
                          unsigned int *r_span_len)
{
  if (arr_len == 0) {
    return false;
  }
  if (use_wrap && (span_step[0] != arr_len) && (span_step[0] > span_step[1])) {
    return false;
  }

  const unsigned int arr_stride_uint = (unsigned int)arr_stride;
  const void *item_prev;
  bool test_prev;

  unsigned int i_curr;

  if ((span_step[0] == arr_len) && (span_step[1] == arr_len)) {
    if (use_wrap) {
      item_prev = POINTER_OFFSET(arr, (arr_len - 1) * arr_stride_uint);
      i_curr = 0;
      test_prev = test_fn(item_prev, user_data);
    }
    else if (use_delimit_bounds == false) {
      item_prev = arr;
      i_curr = 1;
      test_prev = test_fn(item_prev, user_data);
    }
    else {
      item_prev = NULL;
      i_curr = 0;
      test_prev = false;
    }
  }
  else if ((i_curr = span_step[1] + 2) < arr_len) {
    item_prev = POINTER_OFFSET(arr, (span_step[1] + 1) * arr_stride_uint);
    test_prev = test_fn(item_prev, user_data);
  }
  else {
    return false;
  }
  BLI_assert(i_curr < arr_len);

  const void *item_curr = POINTER_OFFSET(arr, i_curr * arr_stride_uint);

  while (i_curr < arr_len) {
    bool test_curr = test_fn(item_curr, user_data);
    if ((test_prev == false) && (test_curr == true)) {
      unsigned int span_len;
      unsigned int i_step_prev = i_curr;

      if (use_wrap) {
        unsigned int i_step = i_curr + 1;
        if (UNLIKELY(i_step == arr_len)) {
          i_step = 0;
        }
        while (test_fn(POINTER_OFFSET(arr, i_step * arr_stride_uint), user_data)) {
          i_step_prev = i_step;
          i_step++;
          if (UNLIKELY(i_step == arr_len)) {
            i_step = 0;
          }
        }

        if (i_step_prev < i_curr) {
          span_len = (i_step_prev + (arr_len - i_curr)) + 1;
        }
        else {
          span_len = (i_step_prev - i_curr) + 1;
        }
      }
      else {
        unsigned int i_step = i_curr + 1;
        while ((i_step != arr_len) &&
               test_fn(POINTER_OFFSET(arr, i_step * arr_stride_uint), user_data)) {
          i_step_prev = i_step;
          i_step++;
        }

        span_len = (i_step_prev - i_curr) + 1;

        if ((use_delimit_bounds == false) && (i_step_prev == arr_len - 1)) {
          return false;
        }
      }

      span_step[0] = i_curr;
      span_step[1] = i_step_prev;
      *r_span_len = span_len;

      return true;
    }

    test_prev = test_curr;

    item_prev = item_curr;
    item_curr = POINTER_OFFSET(item_curr, arr_stride_uint);
    i_curr++;
  }

  return false;
}

/**
 * Simple utility to check memory is zeroed.
 */
bool _bli_array_is_zeroed(const void *arr_v, unsigned int arr_len, size_t arr_stride)
{
  const char *arr_step = (const char *)arr_v;
  size_t i = arr_stride * arr_len;
  while (i--) {
    if (*(arr_step++)) {
      return false;
    }
  }
  return true;
}

/**
 * Smart function to sample a rect spiraling outside.
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
                                   void *user_data)
{
  BLI_assert(center[0] >= 0 && center[1] >= 0 && center[0] < arr_shape[0] &&
             center[1] < arr_shape[1]);

  const char *arr = arr_v;
  const int stride[2] = {arr_shape[1] * (int)elem_size, (int)elem_size};

  /* Test center first. */
  int ofs[2] = {center[0] * stride[0], center[1] * stride[1]};
  if (test_fn(arr + ofs[0] + ofs[1], user_data)) {
    return true;
  }

  /* #steps_in and #steps_out are the "diameters" of the inscribed and circumscribed squares in the
   * rectangle. Each step smaller than #steps_in does not need to check bounds. */
  int steps_in, steps_out;
  {
    int x_minus = center[0];
    int x_plus = arr_shape[0] - center[0] - 1;
    int y_minus = center[1];
    int y_plus = arr_shape[1] - center[1] - 1;

    steps_in = 2 * min_iiii(x_minus, x_plus, y_minus, y_plus);
    steps_out = 2 * max_iiii(x_minus, x_plus, y_minus, y_plus);
  }

  /* For check_bounds. */
  int limits[2] = {(arr_shape[0] - 1) * stride[0], stride[0] - stride[1]};

  int steps = 0;
  while (steps < steps_out) {
    steps += 2;

    /* Move one step to the diagonal of the negative quadrant. */
    ofs[0] -= stride[0];
    ofs[1] -= stride[1];

    bool check_bounds = steps > steps_in;

    /* sign: 0 neg; 1 pos; */
    for (int sign = 2; sign--;) {
      /* axis: 0 x; 1 y; */
      for (int axis = 2; axis--;) {
        int ofs_step = stride[axis];
        if (!sign) {
          ofs_step *= -1;
        }

        int ofs_iter = ofs[axis] + ofs_step;
        int ofs_dest = ofs[axis] + steps * ofs_step;
        int ofs_other = ofs[!axis];

        ofs[axis] = ofs_dest;
        if (check_bounds) {
          if (ofs_other < 0 || ofs_other > limits[!axis]) {
            /* Out of bounds. */
            continue;
          }

          CLAMP(ofs_iter, 0, limits[axis]);
          CLAMP(ofs_dest, 0, limits[axis]);
        }

        while (true) {
          if (test_fn(arr + ofs_other + ofs_iter, user_data)) {
            return true;
          }
          if (ofs_iter == ofs_dest) {
            break;
          }
          ofs_iter += ofs_step;
        }
      }
    }
  }
  return false;
}
