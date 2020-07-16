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
