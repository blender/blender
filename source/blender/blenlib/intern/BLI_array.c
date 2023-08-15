/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief A (mainly) macro array library.
 *
 * This is an array library, used to manage array (re)allocation.
 *
 * \note This is primarily accessed via macros,
 * functions are used to implement some of the internals.
 *
 * Example usage:
 *
 * \code{.c}
 * int *arr = NULL;
 * BLI_array_declare(arr);
 * int i;
 *
 * for (i = 0; i < 10; i++) {
 *     BLI_array_grow_one(arr);
 *     arr[i] = something;
 * }
 * BLI_array_free(arr);
 * \endcode
 *
 * Arrays are over allocated, so each reallocation the array size is doubled.
 * In situations where contiguous array access isn't needed,
 * other solutions for allocation are available.
 * Consider using on of: BLI_memarena.c, BLI_mempool.c, BLi_stack.c
 */

#include <string.h>

#include "BLI_array.h"

#include "BLI_sys_types.h"

#include "MEM_guardedalloc.h"

void _bli_array_grow_func(void **arr_p,
                          const void *arr_static,
                          const int sizeof_arr_p,
                          const int arr_len,
                          const int num,
                          const char *alloc_str)
{
  void *arr = *arr_p;
  void *arr_tmp;

  arr_tmp = MEM_mallocN(sizeof_arr_p * ((num < arr_len) ? (arr_len * 2 + 2) : (arr_len + num)),
                        alloc_str);

  if (arr) {
    memcpy(arr_tmp, arr, sizeof_arr_p * arr_len);

    if (arr != arr_static) {
      MEM_freeN(arr);
    }
  }

  *arr_p = arr_tmp;
}
