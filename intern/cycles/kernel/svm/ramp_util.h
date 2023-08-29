/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* NOTE: svm_ramp.h, svm_ramp_util.h and node_ramp_util.h must stay consistent */

ccl_device_inline float3
rgb_ramp_lookup(const float3 *ramp, float f, bool interpolate, bool extrapolate, int table_size)
{
  if ((f < 0.0f || f > 1.0f) && extrapolate) {
    float3 t0, dy;
    if (f < 0.0f) {
      t0 = ramp[0];
      dy = t0 - ramp[1], f = -f;
    }
    else {
      t0 = ramp[table_size - 1];
      dy = t0 - ramp[table_size - 2];
      f = f - 1.0f;
    }
    return t0 + dy * f * (table_size - 1);
  }

  f = clamp(f, 0.0f, 1.0f) * (table_size - 1);

  /* clamp int as well in case of NaN */
  int i = clamp(float_to_int(f), 0, table_size - 1);
  float t = f - (float)i;

  float3 result = ramp[i];

  if (interpolate && t > 0.0f) {
    result = (1.0f - t) * result + t * ramp[i + 1];
  }

  return result;
}

ccl_device float float_ramp_lookup(
    const float *ramp, float f, bool interpolate, bool extrapolate, int table_size)
{
  if ((f < 0.0f || f > 1.0f) && extrapolate) {
    float t0, dy;
    if (f < 0.0f) {
      t0 = ramp[0];
      dy = t0 - ramp[1], f = -f;
    }
    else {
      t0 = ramp[table_size - 1];
      dy = t0 - ramp[table_size - 2];
      f = f - 1.0f;
    }
    return t0 + dy * f * (table_size - 1);
  }

  f = clamp(f, 0.0f, 1.0f) * (table_size - 1);

  /* clamp int as well in case of NaN */
  int i = clamp(float_to_int(f), 0, table_size - 1);
  float t = f - (float)i;

  float result = ramp[i];

  if (interpolate && t > 0.0f) {
    result = (1.0f - t) * result + t * ramp[i + 1];
  }

  return result;
}

CCL_NAMESPACE_END
