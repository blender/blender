/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <cmath>

#include "BLI_assert.h"
#include "BLI_math_constants.h"
#include "BLI_math_filter.hh"

namespace blender::math {

static float filt_quadratic(float x)
{
  if (x < 0.0f) {
    x = -x;
  }
  if (x < 0.5f) {
    return 0.75f - (x * x);
  }
  if (x < 1.5f) {
    return 0.50f * (x - 1.5f) * (x - 1.5f);
  }
  return 0.0f;
}

static float filt_cubic(float x)
{
  float x2 = x * x;

  if (x < 0.0f) {
    x = -x;
  }

  if (x < 1.0f) {
    return 0.5f * x * x2 - x2 + 2.0f / 3.0f;
  }
  if (x < 2.0f) {
    return (2.0f - x) * (2.0f - x) * (2.0f - x) / 6.0f;
  }
  return 0.0f;
}

static float filt_catrom(float x)
{
  float x2 = x * x;

  if (x < 0.0f) {
    x = -x;
  }
  if (x < 1.0f) {
    return 1.5f * x2 * x - 2.5f * x2 + 1.0f;
  }
  if (x < 2.0f) {
    return -0.5f * x2 * x + 2.5f * x2 - 4.0f * x + 2.0f;
  }
  return 0.0f;
}

static float filt_mitchell(float x)
{
  float b = 1.0f / 3.0f, c = 1.0f / 3.0f;
  float p0 = (6.0f - 2.0f * b) / 6.0f;
  float p2 = (-18.0f + 12.0f * b + 6.0f * c) / 6.0f;
  float p3 = (12.0f - 9.0f * b - 6.0f * c) / 6.0f;
  float q0 = (8.0f * b + 24.0f * c) / 6.0f;
  float q1 = (-12.0f * b - 48.0f * c) / 6.0f;
  float q2 = (6.0f * b + 30.0f * c) / 6.0f;
  float q3 = (-b - 6.0f * c) / 6.0f;

  if (x < -2.0f) {
    return 0.0f;
  }
  if (x < -1.0f) {
    return (q0 - x * (q1 - x * (q2 - x * q3)));
  }
  if (x < 0.0f) {
    return (p0 + x * x * (p2 - x * p3));
  }
  if (x < 1.0f) {
    return (p0 + x * x * (p2 + x * p3));
  }
  if (x < 2.0f) {
    return (q0 + x * (q1 + x * (q2 + x * q3)));
  }
  return 0.0f;
}

float filter_kernel_value(FilterKernel kernel, float x)
{
  constexpr float scale = 1.6f;

  x = fabsf(x);

  switch (kernel) {
    case FilterKernel::Box:
      if (x > 1.0f) {
        return 0.0f;
      }
      return 1.0f;
    case FilterKernel::Tent:
      if (x > 1.0f) {
        return 0.0f;
      }
      return 1.0f - x;
    case FilterKernel::Quad:
      return filt_quadratic(x * scale);
    case FilterKernel::Cubic:
      return filt_cubic(x * scale);
    case FilterKernel::Catrom:
      return filt_catrom(x * scale);
    case FilterKernel::Gauss: {
      constexpr float two_scale2 = 2.0f * scale * scale;
      x *= 3.0f * scale;
      return 1.0f / sqrtf(float(M_PI) * two_scale2) * expf(-x * x / two_scale2);
    }
    case FilterKernel::Mitch:
      return filt_mitchell(x * scale);
    default:
      BLI_assert_unreachable();
  }
  return 0.0f;
}

}  // namespace blender::math
