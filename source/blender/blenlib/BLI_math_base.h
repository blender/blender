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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 */

#pragma once

/** \file
 * \ingroup bli
 */

#if defined(_MSC_VER) && !defined(_USE_MATH_DEFINES)
#  define _USE_MATH_DEFINES
#endif

#include "BLI_assert.h"
#include "BLI_math_inline.h"
#include "BLI_sys_types.h"
#include <math.h>

#ifndef M_PI
#  define M_PI 3.14159265358979323846 /* pi */
#endif
#ifndef M_PI_2
#  define M_PI_2 1.57079632679489661923 /* pi/2 */
#endif
#ifndef M_PI_4
#  define M_PI_4 0.78539816339744830962 /* pi/4 */
#endif
#ifndef M_SQRT2
#  define M_SQRT2 1.41421356237309504880 /* sqrt(2) */
#endif
#ifndef M_SQRT1_2
#  define M_SQRT1_2 0.70710678118654752440 /* 1/sqrt(2) */
#endif
#ifndef M_SQRT3
#  define M_SQRT3 1.73205080756887729352 /* sqrt(3) */
#endif
#ifndef M_SQRT1_3
#  define M_SQRT1_3 0.57735026918962576450 /* 1/sqrt(3) */
#endif
#ifndef M_1_PI
#  define M_1_PI 0.318309886183790671538 /* 1/pi */
#endif
#ifndef M_E
#  define M_E 2.7182818284590452354 /* e */
#endif
#ifndef M_LOG2E
#  define M_LOG2E 1.4426950408889634074 /* log_2 e */
#endif
#ifndef M_LOG10E
#  define M_LOG10E 0.43429448190325182765 /* log_10 e */
#endif
#ifndef M_LN2
#  define M_LN2 0.69314718055994530942 /* log_e 2 */
#endif
#ifndef M_LN10
#  define M_LN10 2.30258509299404568402 /* log_e 10 */
#endif

#if defined(__GNUC__)
#  define NAN_FLT __builtin_nanf("")
#else
/* evil quiet NaN definition */
static const int NAN_INT = 0x7FC00000;
#  define NAN_FLT (*((float *)(&NAN_INT)))
#endif

#if BLI_MATH_DO_INLINE
#  include "intern/math_base_inline.c"
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

#ifdef __cplusplus
extern "C" {
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
MINLINE double interpd(double a, double b, double t);

MINLINE float ratiof(float min, float max, float pos);
MINLINE double ratiod(double min, double max, double pos);

MINLINE float scalenorm(float a, float b, float x);
MINLINE double scalenormd(double a, double b, double x);

/* NOTE: Compilers will upcast all types smaller than int to int when performing arithmetic
 * operation. */
MINLINE int square_s(short a);
MINLINE int square_uchar(unsigned char a);
MINLINE int cube_s(short a);
MINLINE int cube_uchar(unsigned char a);

MINLINE int square_i(int a);
MINLINE unsigned int square_uint(unsigned int a);
MINLINE float square_f(float a);
MINLINE double square_d(double a);

MINLINE int cube_i(int a);
MINLINE unsigned int cube_uint(unsigned int a);
MINLINE float cube_f(float a);
MINLINE double cube_d(double a);

MINLINE float min_ff(float a, float b);
MINLINE float max_ff(float a, float b);
MINLINE float min_fff(float a, float b, float c);
MINLINE float max_fff(float a, float b, float c);
MINLINE float min_ffff(float a, float b, float c, float d);
MINLINE float max_ffff(float a, float b, float c, float d);

MINLINE double min_dd(double a, double b);
MINLINE double max_dd(double a, double b);

MINLINE int min_ii(int a, int b);
MINLINE int max_ii(int a, int b);
MINLINE int min_iii(int a, int b, int c);
MINLINE int max_iii(int a, int b, int c);
MINLINE int min_iiii(int a, int b, int c, int d);
MINLINE int max_iiii(int a, int b, int c, int d);

MINLINE uint min_uu(uint a, uint b);
MINLINE uint max_uu(uint a, uint b);

MINLINE size_t min_zz(size_t a, size_t b);
MINLINE size_t max_zz(size_t a, size_t b);

MINLINE char min_cc(char a, char b);
MINLINE char max_cc(char a, char b);

MINLINE int clamp_i(int value, int min, int max);
MINLINE float clamp_f(float value, float min, float max);
MINLINE size_t clamp_z(size_t value, size_t min, size_t max);

MINLINE int compare_ff(float a, float b, const float max_diff);
MINLINE int compare_ff_relative(float a, float b, const float max_diff, const int max_ulps);
MINLINE bool compare_threshold_relative(const float value1,
                                        const float value2,
                                        const float thresh);

MINLINE float signf(float f);
MINLINE int signum_i_ex(float a, float eps);
MINLINE int signum_i(float a);

MINLINE float power_of_2(float f);

MINLINE int integer_digits_f(const float f);
MINLINE int integer_digits_d(const double d);
MINLINE int integer_digits_i(const int i);

/* these don't really fit anywhere but were being copied about a lot */
MINLINE int is_power_of_2_i(int n);
MINLINE int power_of_2_max_i(int n);
MINLINE int power_of_2_min_i(int n);

MINLINE unsigned int power_of_2_max_u(unsigned int x);
MINLINE unsigned int power_of_2_min_u(unsigned int x);
MINLINE unsigned int log2_floor_u(unsigned int x);
MINLINE unsigned int log2_ceil_u(unsigned int x);

MINLINE int divide_round_i(int a, int b);
MINLINE int mod_i(int i, int n);

MINLINE float round_to_even(float f);

MINLINE signed char round_fl_to_char(float a);
MINLINE unsigned char round_fl_to_uchar(float a);
MINLINE short round_fl_to_short(float a);
MINLINE unsigned short round_fl_to_ushort(float a);
MINLINE int round_fl_to_int(float a);
MINLINE unsigned int round_fl_to_uint(float a);

MINLINE signed char round_db_to_char(double a);
MINLINE unsigned char round_db_to_uchar(double a);
MINLINE short round_db_to_short(double a);
MINLINE unsigned short round_db_to_ushort(double a);
MINLINE int round_db_to_int(double a);
MINLINE unsigned int round_db_to_uint(double a);

MINLINE signed char round_fl_to_char_clamp(float a);
MINLINE unsigned char round_fl_to_uchar_clamp(float a);
MINLINE short round_fl_to_short_clamp(float a);
MINLINE unsigned short round_fl_to_ushort_clamp(float a);
MINLINE int round_fl_to_int_clamp(float a);
MINLINE unsigned int round_fl_to_uint_clamp(float a);

MINLINE signed char round_db_to_char_clamp(double a);
MINLINE unsigned char round_db_to_uchar_clamp(double a);
MINLINE short round_db_to_short_clamp(double a);
MINLINE unsigned short round_db_to_ushort_clamp(double a);
MINLINE int round_db_to_int_clamp(double a);
MINLINE unsigned int round_db_to_uint_clamp(double a);

int pow_i(int base, int exp);
double double_round(double x, int ndigits);

float floor_power_of_10(float f);
float ceil_power_of_10(float f);

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

/* asserts, some math functions expect normalized inputs
 * check the vector is unit length, or zero length (which can't be helped in some cases).
 */
#ifndef NDEBUG
/** \note 0.0001 is too small because normals may be converted from short's: see T34322. */
#  define BLI_ASSERT_UNIT_EPSILON 0.0002f
#  define BLI_ASSERT_UNIT_EPSILON_DB 0.0002
/**
 * \note Checks are flipped so NAN doesn't assert.
 * This is done because we're making sure the value was normalized and in the case we
 * don't want NAN to be raising asserts since there is nothing to be done in that case.
 */
#  define BLI_ASSERT_UNIT_V3(v) \
    { \
      const float _test_unit = len_squared_v3(v); \
      BLI_assert(!(fabsf(_test_unit - 1.0f) >= BLI_ASSERT_UNIT_EPSILON) || \
                 !(fabsf(_test_unit) >= BLI_ASSERT_UNIT_EPSILON)); \
    } \
    (void)0

#  define BLI_ASSERT_UNIT_V3_DB(v) \
    { \
      const double _test_unit = len_squared_v3_db(v); \
      BLI_assert(!(fabs(_test_unit - 1.0) >= BLI_ASSERT_UNIT_EPSILON_DB) || \
                 !(fabs(_test_unit) >= BLI_ASSERT_UNIT_EPSILON_DB)); \
    } \
    (void)0

#  define BLI_ASSERT_UNIT_V2(v) \
    { \
      const float _test_unit = len_squared_v2(v); \
      BLI_assert(!(fabsf(_test_unit - 1.0f) >= BLI_ASSERT_UNIT_EPSILON) || \
                 !(fabsf(_test_unit) >= BLI_ASSERT_UNIT_EPSILON)); \
    } \
    (void)0

#  define BLI_ASSERT_UNIT_QUAT(q) \
    { \
      const float _test_unit = dot_qtqt(q, q); \
      BLI_assert(!(fabsf(_test_unit - 1.0f) >= BLI_ASSERT_UNIT_EPSILON * 10) || \
                 !(fabsf(_test_unit) >= BLI_ASSERT_UNIT_EPSILON * 10)); \
    } \
    (void)0

#  define BLI_ASSERT_ZERO_M3(m) \
    { \
      BLI_assert(dot_vn_vn((const float *)m, (const float *)m, 9) != 0.0); \
    } \
    (void)0

#  define BLI_ASSERT_ZERO_M4(m) \
    { \
      BLI_assert(dot_vn_vn((const float *)m, (const float *)m, 16) != 0.0); \
    } \
    (void)0
#  define BLI_ASSERT_UNIT_M3(m) \
    { \
      BLI_ASSERT_UNIT_V3((m)[0]); \
      BLI_ASSERT_UNIT_V3((m)[1]); \
      BLI_ASSERT_UNIT_V3((m)[2]); \
    } \
    (void)0
#else
#  define BLI_ASSERT_UNIT_V2(v) (void)(v)
#  define BLI_ASSERT_UNIT_V3(v) (void)(v)
#  define BLI_ASSERT_UNIT_V3_DB(v) (void)(v)
#  define BLI_ASSERT_UNIT_QUAT(v) (void)(v)
#  define BLI_ASSERT_ZERO_M3(m) (void)(m)
#  define BLI_ASSERT_ZERO_M4(m) (void)(m)
#  define BLI_ASSERT_UNIT_M3(m) (void)(m)
#endif

#ifdef __cplusplus
}
#endif
