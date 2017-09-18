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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#ifndef __BLI_MATH_BASE_H__
#define __BLI_MATH_BASE_H__

/** \file BLI_math_base.h
 *  \ingroup bli
 */

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include <math.h>
#include "BLI_math_inline.h"

#ifndef M_PI
#define M_PI        3.14159265358979323846  /* pi */
#endif
#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923  /* pi/2 */
#endif
#ifndef M_PI_4
#define M_PI_4      0.78539816339744830962  /* pi/4 */
#endif
#ifndef M_SQRT2
#define M_SQRT2     1.41421356237309504880  /* sqrt(2) */
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2   0.70710678118654752440  /* 1/sqrt(2) */
#endif
#ifndef M_SQRT3
#define M_SQRT3     1.73205080756887729352  /* sqrt(3) */
#endif
#ifndef M_SQRT1_3
#define M_SQRT1_3   0.57735026918962576450  /* 1/sqrt(3) */
#endif
#ifndef M_1_PI
#define M_1_PI      0.318309886183790671538  /* 1/pi */
#endif
#ifndef M_E
#define M_E         2.7182818284590452354  /* e */
#endif
#ifndef M_LOG2E
#define M_LOG2E     1.4426950408889634074  /* log_2 e */
#endif
#ifndef M_LOG10E
#define M_LOG10E    0.43429448190325182765  /* log_10 e */
#endif
#ifndef M_LN2
#define M_LN2       0.69314718055994530942  /* log_e 2 */
#endif
#ifndef M_LN10
#define M_LN10      2.30258509299404568402  /* log_e 10 */
#endif

#if defined(__GNUC__)
#  define NAN_FLT __builtin_nanf("")
#else
/* evil quiet NaN definition */
static const int NAN_INT = 0x7FC00000;
#  define NAN_FLT  (*((float *)(&NAN_INT)))
#endif

#if BLI_MATH_DO_INLINE
#include "intern/math_base_inline.c"
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

/******************************* Float ******************************/

MINLINE float pow2f(float x);
MINLINE float pow3f(float x);
MINLINE float pow4f(float x);
MINLINE float pow7f(float x);

MINLINE float sqrt3f(float f);
MINLINE double sqrt3d(double d);

MINLINE float sqrtf_signed(float f);

MINLINE float saacosf(float f);
MINLINE float saasinf(float f);
MINLINE float sasqrtf(float f);
MINLINE float saacos(float fac);
MINLINE float saasin(float fac);
MINLINE float sasqrt(float fac);

MINLINE float interpf(float a, float b, float t);

MINLINE float min_ff(float a, float b);
MINLINE float max_ff(float a, float b);
MINLINE float min_fff(float a, float b, float c);
MINLINE float max_fff(float a, float b, float c);
MINLINE float min_ffff(float a, float b, float c, float d);
MINLINE float max_ffff(float a, float b, float c, float d);

MINLINE int min_ii(int a, int b);
MINLINE int max_ii(int a, int b);
MINLINE int min_iii(int a, int b, int c);
MINLINE int max_iii(int a, int b, int c);
MINLINE int min_iiii(int a, int b, int c, int d);
MINLINE int max_iiii(int a, int b, int c, int d);

MINLINE int compare_ff(float a, float b, const float max_diff);
MINLINE int compare_ff_relative(float a, float b, const float max_diff, const int max_ulps);

MINLINE float signf(float f);
MINLINE int signum_i_ex(float a, float eps);
MINLINE int signum_i(float a);

MINLINE float power_of_2(float f);

MINLINE int integer_digits_f(const float f);
MINLINE int integer_digits_d(const double d);

/* these don't really fit anywhere but were being copied about a lot */
MINLINE int is_power_of_2_i(int n);
MINLINE int power_of_2_max_i(int n);
MINLINE int power_of_2_min_i(int n);

MINLINE unsigned int power_of_2_max_u(unsigned int x);
MINLINE unsigned int power_of_2_min_u(unsigned int x);

MINLINE int iroundf(float a);
MINLINE int divide_round_i(int a, int b);
MINLINE int mod_i(int i, int n);

MINLINE signed char    round_fl_to_char_clamp(float a);
MINLINE unsigned char  round_fl_to_uchar_clamp(float a);
MINLINE short          round_fl_to_short_clamp(float a);
MINLINE unsigned short round_fl_to_ushort_clamp(float a);
MINLINE int            round_fl_to_int_clamp(float a);
MINLINE unsigned int   round_fl_to_uint_clamp(float a);

MINLINE signed char    round_db_to_char_clamp(double a);
MINLINE unsigned char  round_db_to_uchar_clamp(double a);
MINLINE short          round_db_to_short_clamp(double a);
MINLINE unsigned short round_db_to_ushort_clamp(double a);
MINLINE int            round_db_to_int_clamp(double a);
MINLINE unsigned int   round_db_to_uint_clamp(double a);

int pow_i(int base, int exp);
double double_round(double x, int ndigits);

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

/* asserts, some math functions expect normalized inputs
 * check the vector is unit length, or zero length (which can't be helped in some cases).
 */
#ifndef NDEBUG
/* note: 0.0001 is too small becaues normals may be converted from short's: see [#34322] */
#  define BLI_ASSERT_UNIT_EPSILON 0.0002f
#  define BLI_ASSERT_UNIT_V3(v)  {                                            \
	const float _test_unit = len_squared_v3(v);                               \
	BLI_assert((fabsf(_test_unit - 1.0f) < BLI_ASSERT_UNIT_EPSILON) ||        \
	           (fabsf(_test_unit)        < BLI_ASSERT_UNIT_EPSILON));         \
} (void)0

#  define BLI_ASSERT_UNIT_V2(v)  {                                            \
	const float _test_unit = len_squared_v2(v);                               \
	BLI_assert((fabsf(_test_unit - 1.0f) < BLI_ASSERT_UNIT_EPSILON) ||        \
	           (fabsf(_test_unit)        < BLI_ASSERT_UNIT_EPSILON));         \
} (void)0

#  define BLI_ASSERT_UNIT_QUAT(q)  {                                          \
	const float _test_unit = dot_qtqt(q, q);                                  \
	BLI_assert((fabsf(_test_unit - 1.0f) < BLI_ASSERT_UNIT_EPSILON * 10) ||   \
	           (fabsf(_test_unit)        < BLI_ASSERT_UNIT_EPSILON * 10));    \
} (void)0

#  define BLI_ASSERT_ZERO_M3(m)  {                                            \
	BLI_assert(dot_vn_vn((const float *)m, (const float *)m, 9) != 0.0);      \
} (void)0

#  define BLI_ASSERT_ZERO_M4(m)  {                                            \
	BLI_assert(dot_vn_vn((const float *)m, (const float *)m, 16) != 0.0);     \
} (void)0
#  define BLI_ASSERT_UNIT_M3(m)  {                                            \
	BLI_ASSERT_UNIT_V3((m)[0]);                                               \
	BLI_ASSERT_UNIT_V3((m)[1]);                                               \
	BLI_ASSERT_UNIT_V3((m)[2]);                                               \
} (void)0
#else
#  define BLI_ASSERT_UNIT_V2(v) (void)(v)
#  define BLI_ASSERT_UNIT_V3(v) (void)(v)
#  define BLI_ASSERT_UNIT_QUAT(v) (void)(v)
#  define BLI_ASSERT_ZERO_M3(m) (void)(m)
#  define BLI_ASSERT_ZERO_M4(m) (void)(m)
#  define BLI_ASSERT_UNIT_M3(m) (void)(m)
#endif

#endif /* __BLI_MATH_BASE_H__ */
