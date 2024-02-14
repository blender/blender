/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief Generic memory manipulation API.
 *
 * This is to extend on existing functions
 * such as `memcpy` & `memcmp`.
 */
#include <string.h>

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "BLI_memory_utils.h"

#include "BLI_strict_flags.h" /* Keep last. */

bool BLI_memory_is_zero(const void *arr, const size_t arr_size)
{
  const char *arr_byte = arr;
  const char *arr_end = (const char *)arr + arr_size;

  while ((arr_byte != arr_end) && (*arr_byte == 0)) {
    arr_byte++;
  }

  return (arr_byte == arr_end);
}
