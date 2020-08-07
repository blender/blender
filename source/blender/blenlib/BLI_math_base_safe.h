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

#pragma once

/** \file
 * \ingroup bli
 *
 * This file provides safe alternatives to common math functions like sqrt, powf.
 * In this context "safe" means that the output is not NaN if the input is not NaN.
 */

#include "BLI_math_base.h"

#ifdef __cplusplus
extern "C" {
#endif

MINLINE float safe_divide(float a, float b);
MINLINE float safe_modf(float a, float b);
MINLINE float safe_logf(float a, float base);
MINLINE float safe_sqrtf(float a);
MINLINE float safe_inverse_sqrtf(float a);
MINLINE float safe_asinf(float a);
MINLINE float safe_acosf(float a);
MINLINE float safe_powf(float base, float exponent);

#ifdef __cplusplus
}
#endif

#if BLI_MATH_DO_INLINE
#  include "intern/math_base_safe_inline.c"
#endif
