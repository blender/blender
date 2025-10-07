/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_math_safe_lib.glsl"

#define CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA 0
#define CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA 1

void node_composite_distance_matte(const float4 color,
                                   const float4 key,
                                   const float color_space,
                                   const float tolerance,
                                   const float falloff,
                                   out float4 result,
                                   out float matte)
{
  float4 color_vector = color;
  float4 key_vector = key;
  switch (int(color_space)) {
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA:
      color_vector = color;
      key_vector = key;
      break;
    case CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA:
      rgba_to_ycca_itu_709(color, color_vector);
      rgba_to_ycca_itu_709(key, key_vector);
      break;
  }

  float difference = distance(color_vector.xyz(), key_vector.xyz());
  bool is_opaque = difference > tolerance + falloff;
  float alpha = is_opaque ? color.w : safe_divide(max(0.0f, difference - tolerance), falloff);
  matte = min(alpha, color.w);
  result = color * matte;
}

#undef CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_RGBA
#undef CMP_NODE_DISTANCE_MATTE_COLOR_SPACE_YCCA
