/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 accumulated_color = float4(0.0f);
  float4 accumulated_weight = float4(0.0f);

  const float2 size = max(float2(0.0), texture_load(size_tx, texel).xy);
  int2 radius = int2(ceil(size));
  float2 coordinates_scale = float2(1.0f) / (size + float2(1));

  /* First, compute the contribution of the center pixel. */
  float4 center_color = texture_load(input_tx, texel);
  float center_weight = texture_load(weights_tx, int2(0)).x;
  accumulated_color += center_color * center_weight;
  accumulated_weight += center_weight;

  /* Then, compute the contributions of the pixels along the x axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int x = 1; x <= radius.x; x++) {
    float weight_coordinates = (x + 0.5f) * coordinates_scale.x;
    float weight = texture(weights_tx, float2(weight_coordinates, 0.0f)).x;
    accumulated_color += texture_load(input_tx, texel + int2(x, 0)) * weight;
    accumulated_color += texture_load(input_tx, texel + int2(-x, 0)) * weight;
    accumulated_weight += weight * 2.0f;
  }

  /* Then, compute the contributions of the pixels along the y axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int y = 1; y <= radius.y; y++) {
    float weight_coordinates = (y + 0.5f) * coordinates_scale.y;
    float weight = texture(weights_tx, float2(0.0f, weight_coordinates)).x;
    accumulated_color += texture_load(input_tx, texel + int2(0, y)) * weight;
    accumulated_color += texture_load(input_tx, texel + int2(0, -y)) * weight;
    accumulated_weight += weight * 2.0f;
  }

  /* Finally, compute the contributions of the pixels in the four quadrants of the filter, noting
   * that the weights texture only stores the weights for the upper right quadrant, but since the
   * filter is symmetric, the same weight is used for the rest of the quadrants and we add all four
   * of their contributions. */
  for (int y = 1; y <= radius.y; y++) {
    for (int x = 1; x <= radius.x; x++) {
      float2 weight_coordinates = (float2(x, y) + float2(0.5f)) * coordinates_scale;
      float weight = texture(weights_tx, weight_coordinates).x;
      accumulated_color += texture_load(input_tx, texel + int2(x, y)) * weight;
      accumulated_color += texture_load(input_tx, texel + int2(-x, y)) * weight;
      accumulated_color += texture_load(input_tx, texel + int2(x, -y)) * weight;
      accumulated_color += texture_load(input_tx, texel + int2(-x, -y)) * weight;
      accumulated_weight += weight * 4.0f;
    }
  }

  accumulated_color = safe_divide(accumulated_color, accumulated_weight);

  imageStore(output_img, texel, accumulated_color);
}
