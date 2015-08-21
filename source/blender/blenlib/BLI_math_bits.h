/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef __BLI_MATH_BITS_H__
#define __BLI_MATH_BITS_H__

/** \file BLI_math_bits.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_math_inline.h"

MINLINE unsigned int highest_order_bit_i(unsigned int n);
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
#include "intern/math_bits_inline.c"
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BLI_MATH_BITS_H__ */
