/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
MINLINE unsigned int bitscan_forward_uint64(unsigned long long a);

/* Similar to above, but also clears the bit. */

MINLINE int bitscan_forward_clear_i(int *a);
MINLINE unsigned int bitscan_forward_clear_uint(unsigned int *a);

/* Search the value from MSB to LSB for a set bit. Returns index of this bit. */

MINLINE int bitscan_reverse_i(int a);
MINLINE unsigned int bitscan_reverse_uint(unsigned int a);
MINLINE unsigned int bitscan_reverse_uint64(unsigned long long a);

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
