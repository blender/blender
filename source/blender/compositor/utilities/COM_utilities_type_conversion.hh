/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "IMB_colormanagement.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Float to other.
 */

inline int float_to_int(const float &value)
{
  return int(value);
}

inline float4 float_to_vector(const float &value)
{
  return float4(float3(value), 1.0f);
}

inline float4 float_to_color(const float &value)
{
  return float4(float3(value), 1.0f);
}

/* --------------------------------------------------------------------
 * Int to other.
 */

inline float int_to_float(const int &value)
{
  return float(value);
}

inline float4 int_to_vector(const int &value)
{
  return float_to_vector(int_to_float(value));
}

inline float4 int_to_color(const int &value)
{
  return float_to_color(int_to_float(value));
}

/* --------------------------------------------------------------------
 * Vector to other.
 */

inline float vector_to_float(const float4 &value)
{
  return math::reduce_add(value.xyz()) / 3.0f;
}

inline int vector_to_int(const float4 &value)
{
  return float_to_int(vector_to_float(value));
}

inline float4 vector_to_color(const float4 &value)
{
  return float4(value.xyz(), 1.0f);
}

/* --------------------------------------------------------------------
 * Color to other.
 */

inline float color_to_float(const float4 &value)
{
  return IMB_colormanagement_get_luminance(value);
}

inline int color_to_int(const float4 &value)
{
  return float_to_int(color_to_float(value));
}

inline float4 color_to_vector(const float4 &value)
{
  return value;
}

}  // namespace blender::compositor
