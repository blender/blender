/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __MATH_BASE_SAFE_INLINE_C__
#define __MATH_BASE_SAFE_INLINE_C__

#include "BLI_math_base_safe.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

MINLINE float safe_divide(float a, float b)
{
  return (b != 0.0f) ? a / b : 0.0f;
}

MINLINE float safe_modf(float a, float b)
{
  return (b != 0.0f) ? fmodf(a, b) : 0.0f;
}

MINLINE float safe_floored_modf(float a, float b)
{
  return (b != 0.0f) ? a - floorf(a / b) * b : 0.0f;
}

MINLINE float safe_logf(float a, float base)
{
  if (UNLIKELY(a <= 0.0f || base <= 0.0f)) {
    return 0.0f;
  }
  return safe_divide(logf(a), logf(base));
}

MINLINE float safe_sqrtf(float a)
{
  return sqrtf(MAX2(a, 0.0f));
}

MINLINE float safe_inverse_sqrtf(float a)
{
  return (a > 0.0f) ? 1.0f / sqrtf(a) : 0.0f;
}

MINLINE float safe_asinf(float a)
{
  CLAMP(a, -1.0f, 1.0f);
  return asinf(a);
}

MINLINE float safe_acosf(float a)
{
  CLAMP(a, -1.0f, 1.0f);
  return acosf(a);
}

MINLINE float safe_powf(float base, float exponent)
{
  if (UNLIKELY(base < 0.0f && exponent != (int)exponent)) {
    return 0.0f;
  }
  return powf(base, exponent);
}

#ifdef __cplusplus
}
#endif

#endif /* __MATH_BASE_SAFE_INLINE_C__ */
