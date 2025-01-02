/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

namespace blender::compositor {

inline float4 float_to_vector(const float &input)
{
  return float4(float3(input), 1.0f);
}

inline float4 float_to_color(const float &input)
{
  return float4(float3(input), 1.0f);
}

inline float vector_to_float(const float4 &input)
{
  return math::reduce_add(input.xyz()) / 3.0f;
}

inline float4 vector_to_color(const float4 &input)
{
  return float4(input.xyz(), 1.0f);
}

inline float color_to_float(const float4 &input)
{
  return math::reduce_add(input.xyz()) / 3.0f;
}

inline float4 color_to_vector(const float4 &input)
{
  return input;
}

}  // namespace blender::compositor
