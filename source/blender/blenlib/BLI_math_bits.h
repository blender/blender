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
 *
 * */

#ifndef __BLI_MATH_BITS_H__
#define __BLI_MATH_BITS_H__

/** \file
 * \ingroup bli
 */

#include "BLI_math_inline.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Search the value from LSB to MSB for a set bit. Returns index of this bit. */
MINLINE int bitscan_forward_i(int a);
MINLINE unsigned int bitscan_forward_uint(unsigned int a);

/* Similar to above, but also clears the bit. */
MINLINE int bitscan_forward_clear_i(int *a);
MINLINE unsigned int bitscan_forward_clear_uint(unsigned int *a);

/* Search the value from MSB to LSB for a set bit. Returns index of this bit. */
MINLINE int bitscan_reverse_i(int a);
MINLINE unsigned int bitscan_reverse_uint(unsigned int a);

/* Similar to above, but also clears the bit. */
MINLINE int bitscan_reverse_clear_i(int *a);
MINLINE unsigned int bitscan_reverse_clear_uint(unsigned int *a);

/* NOTE: Those functions returns 2 to the power of index of highest order bit. */
MINLINE unsigned int highest_order_bit_uint(unsigned int n);
MINLINE unsigned short highest_order_bit_s(unsigned short n);

#ifdef __GNUC__
#  define count_bits_i(i) __builtin_popcount(i)
#else
MINLINE int count_bits_i(unsigned int n);
#endif

MINLINE int float_as_int(float f);
MINLINE unsigned int float_as_uint(float f);
MINLINE float int_as_float(int i);
MINLINE float uint_as_float(unsigned int i);
MINLINE float xor_fl(float x, int y);

#if BLI_MATH_DO_INLINE
#  include "intern/math_bits_inline.c"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_BITS_H__ */
