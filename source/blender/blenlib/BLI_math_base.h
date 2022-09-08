/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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
#else /* evil quiet NaN definition */
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

/* `powf` is really slow for raising to integer powers. */

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

/**
 * Map a normalized value, i.e. from interval [0, 1] to interval [a, b].
 */
MINLINE float scalenorm(float a, float b, float x);
/**
 * Map a normalized value, i.e. from interval [0, 1] to interval [a, b].
 */
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

/**
 * Almost-equal for IEEE floats, using absolute difference method.
 *
 * \param max_diff: the maximum absolute difference.
 */
MINLINE int compare_ff(float a, float b, float max_diff);
/**
 * Almost-equal for IEEE floats, using their integer representation
 * (mixing ULP and absolute difference methods).
 *
 * \param max_diff: is the maximum absolute difference (allows to take care of the near-zero area,
 * where relative difference methods cannot really work).
 * \param max_ulps: is the 'maximum number of floats + 1'
 * allowed between \a a and \a b to consider them equal.
 *
 * \see https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
 */
MINLINE int compare_ff_relative(float a, float b, float max_diff, int max_ulps);
MINLINE bool compare_threshold_relative(float value1, float value2, float thresh);

MINLINE float signf(float f);
MINLINE int signum_i_ex(float a, float eps);
MINLINE int signum_i(float a);

/**
 * Used for zoom values.
 */
MINLINE float power_of_2(float f);

/**
 * Returns number of (base ten) *significant* digits of integer part of given float
 * (negative in case of decimal-only floats, 0.01 returns -1 e.g.).
 */
MINLINE int integer_digits_f(float f);
/**
 * Returns number of (base ten) *significant* digits of integer part of given double
 * (negative in case of decimal-only floats, 0.01 returns -1 e.g.).
 */
MINLINE int integer_digits_d(double d);
MINLINE int integer_digits_i(int i);

/* These don't really fit anywhere but were being copied about a lot. */

MINLINE int is_power_of_2_i(int n);

MINLINE unsigned int log2_floor_u(unsigned int x);
MINLINE unsigned int log2_ceil_u(unsigned int x);

/**
 * Returns next (or previous) power of 2 or the input number if it is already a power of 2.
 */
MINLINE int power_of_2_max_i(int n);
MINLINE int power_of_2_min_i(int n);
MINLINE unsigned int power_of_2_max_u(unsigned int x);
MINLINE unsigned int power_of_2_min_u(unsigned int x);

/**
 * Integer division that rounds 0.5 up, particularly useful for color blending
 * with integers, to avoid gradual darkening when rounding down.
 */
MINLINE int divide_round_i(int a, int b);

/**
 * Integer division that returns the ceiling, instead of flooring like normal C division.
 */
MINLINE uint divide_ceil_u(uint a, uint b);
MINLINE uint64_t divide_ceil_ul(uint64_t a, uint64_t b);

/**
 * Returns \a a if it is a multiple of \a b or the next multiple or \a b after \b a .
 */
MINLINE uint ceil_to_multiple_u(uint a, uint b);
MINLINE uint64_t ceil_to_multiple_ul(uint64_t a, uint64_t b);

/**
 * modulo that handles negative numbers, works the same as Python's.
 */
MINLINE int mod_i(int i, int n);

/**
 * Round to closest even number, halfway cases are rounded away from zero.
 */
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

/**
 * \param ndigits: must be between 0 and 21.
 */
double double_round(double x, int ndigits);

/**
 * Floor to the nearest power of 10, e.g.:
 * - 15.0 -> 10.0
 * - 0.015 -> 0.01
 * - 1.0 -> 1.0
 *
 * \param f: Value to floor, must be over 0.0.
 * \note If we wanted to support signed values we could if this becomes necessary.
 */
float floor_power_of_10(float f);
/**
 * Ceiling to the nearest power of 10, e.g.:
 * - 15.0 -> 100.0
 * - 0.015 -> 0.1
 * - 1.0 -> 1.0
 *
 * \param f: Value to ceiling, must be over 0.0.
 * \note If we wanted to support signed values we could if this becomes necessary.
 */
float ceil_power_of_10(float f);

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

/* Asserts, some math functions expect normalized inputs
 * check the vector is unit length, or zero length (which can't be helped in some cases). */

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
