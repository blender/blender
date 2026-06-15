/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

/* On Windows we have to include math.h before defining our own constants, to
 * avoid warnings about redefinition. This can likely be simplified later when
 * code is fully converted to C++, not defining our own constants at all. */
#if defined(_MSC_VER) && !defined(_USE_MATH_DEFINES)
#  define _USE_MATH_DEFINES
#endif

#include <math.h> /* IWYU pragma: export */

#ifndef M_PI
#  define M_PI 3.14159265358979323846 /* `pi` */
#endif
#ifndef M_TAU
#  define M_TAU 6.28318530717958647692 /* `tau = 2*pi` */
#endif
#ifndef M_PI_2
#  define M_PI_2 1.57079632679489661923 /* `pi/2` */
#endif
#ifndef M_PI_4
#  define M_PI_4 0.78539816339744830962 /* `pi/4` */
#endif
#ifndef M_SQRT2
#  define M_SQRT2 1.41421356237309504880 /* `sqrt(2)` */
#endif
#ifndef M_SQRT1_2
#  define M_SQRT1_2 0.70710678118654752440 /* `1/sqrt(2)` */
#endif
#ifndef M_SQRT3
#  define M_SQRT3 1.73205080756887729352 /* `sqrt(3)` */
#endif
#ifndef M_SQRT1_3
#  define M_SQRT1_3 0.57735026918962576450 /* `1/sqrt(3)` */
#endif
#ifndef M_1_PI
#  define M_1_PI 0.318309886183790671538 /* `1/pi` */
#endif
#ifndef M_E
#  define M_E 2.7182818284590452354 /* `e` */
#endif
#ifndef M_LOG2E
#  define M_LOG2E 1.4426950408889634074 /* `log_2 e` */
#endif
#ifndef M_LOG10E
#  define M_LOG10E 0.43429448190325182765 /* `log_10 e` */
#endif
#ifndef M_LN2
#  define M_LN2 0.69314718055994530942 /* `log_e 2` */
#endif
#ifndef M_LN10
#  define M_LN10 2.30258509299404568402 /* `log_e 10` */
#endif

#if defined(_MSC_VER) && !defined(_MATH_DEFINES_DEFINED)
#  define _MATH_DEFINES_DEFINED
#endif

/* -------------------------------------------------------------------- */
/** \name Conversion Defines
 * \{ */

#define RAD2DEG(_rad) ((_rad) * (180.0 / M_PI))
#define DEG2RAD(_deg) ((_deg) * (M_PI / 180.0))

#define RAD2DEGF(_rad) ((_rad) * (float)(180.0 / M_PI))
#define DEG2RADF(_deg) ((_deg) * (float)(M_PI / 180.0))

/** \} */
