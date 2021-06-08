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
 * \brief Generic memory manipulation API.
 *
 * This is to extend on existing functions
 * such as ``memcpy`` & ``memcmp``.
 */
#include <string.h>

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "BLI_memory_utils.h"

#include "BLI_strict_flags.h"

/**
 * Check if memory is zeroed, as with `memset(arr, 0, arr_size)`.
 */
bool BLI_memory_is_zero(const void *arr, const size_t arr_size)
{
  const char *arr_byte = arr;
  const char *arr_end = (const char *)arr + arr_size;

  while ((arr_byte != arr_end) && (*arr_byte == 0)) {
    arr_byte++;
  }

  return (arr_byte == arr_end);
}
