/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This file provides safe alternatives to common math functions like `sqrt`, `powf`.
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
