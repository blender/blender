/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "IMB_colormanagement.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Float to other.
 */

inline int32_t float_to_int(const float &value)
{
  return int32_t(value);
}

inline float3 float_to_float3(const float &value)
{
  return float3(value);
}

inline float4 float_to_color(const float &value)
{
  return float4(float3(value), 1.0f);
}

inline float4 float_to_float4(const float &value)
{
  return float4(value);
}

/* --------------------------------------------------------------------
 * Int to other.
 */

inline float int_to_float(const int32_t &value)
{
  return float(value);
}

inline float3 int_to_float3(const int32_t &value)
{
  return float_to_float3(int_to_float(value));
}

inline float4 int_to_color(const int32_t &value)
{
  return float_to_color(int_to_float(value));
}

inline float4 int_to_float4(const int32_t &value)
{
  return float_to_float4(int_to_float(value));
}

/* --------------------------------------------------------------------
 * Float3 to other.
 */

inline float float3_to_float(const float3 &value)
{
  return math::reduce_add(value) / 3.0f;
}

inline int32_t float3_to_int(const float3 &value)
{
  return float_to_int(float3_to_float(value));
}

inline float4 float3_to_color(const float3 &value)
{
  return float4(value.xyz(), 1.0f);
}

inline float4 float3_to_float4(const float3 &value)
{
  return float4(value, 0.0f);
}

/* --------------------------------------------------------------------
 * Color to other.
 */

inline float color_to_float(const float4 &value)
{
  return IMB_colormanagement_get_luminance(value);
}

inline int32_t color_to_int(const float4 &value)
{
  return float_to_int(color_to_float(value));
}

inline float3 color_to_float3(const float4 &value)
{
  return value.xyz();
}

inline float4 color_to_float4(const float4 &value)
{
  return value;
}

/* --------------------------------------------------------------------
 * Float4 to other.
 */

inline float float4_to_float(const float4 &value)
{
  return math::reduce_add(value) / 4.0f;
}

inline int32_t float4_to_int(const float4 &value)
{
  return float_to_int(float4_to_float(value));
}

inline float3 float4_to_float3(const float4 &value)
{
  return value.xyz();
}

inline float4 float4_to_color(const float4 &value)
{
  return value;
}

}  // namespace blender::compositor
