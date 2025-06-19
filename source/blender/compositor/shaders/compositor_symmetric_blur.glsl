/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float4 accumulated_color = float4(0.0f);

  /* First, compute the contribution of the center pixel. */
  float4 center_color = texture_load(input_tx, texel);
  accumulated_color += center_color * texture_load(weights_tx, int2(0)).x;

  int2 weights_size = texture_size(weights_tx);

  /* Then, compute the contributions of the pixels along the x axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int x = 1; x < weights_size.x; x++) {
    float weight = texture_load(weights_tx, int2(x, 0)).x;
    accumulated_color += texture_load(input_tx, texel + int2(x, 0)) * weight;
    accumulated_color += texture_load(input_tx, texel + int2(-x, 0)) * weight;
  }

  /* Then, compute the contributions of the pixels along the y axis of the filter, noting that the
   * weights texture only stores the weights for the positive half, but since the filter is
   * symmetric, the same weight is used for the negative half and we add both of their
   * contributions. */
  for (int y = 1; y < weights_size.y; y++) {
    float weight = texture_load(weights_tx, int2(0, y)).x;
    accumulated_color += texture_load(input_tx, texel + int2(0, y)) * weight;
    accumulated_color += texture_load(input_tx, texel + int2(0, -y)) * weight;
  }

  /* Finally, compute the contributions of the pixels in the four quadrants of the filter, noting
   * that the weights texture only stores the weights for the upper right quadrant, but since the
   * filter is symmetric, the same weight is used for the rest of the quadrants and we add all four
   * of their contributions. */
  for (int y = 1; y < weights_size.y; y++) {
    for (int x = 1; x < weights_size.x; x++) {
      float weight = texture_load(weights_tx, int2(x, y)).x;
      accumulated_color += texture_load(input_tx, texel + int2(x, y)) * weight;
      accumulated_color += texture_load(input_tx, texel + int2(-x, y)) * weight;
      accumulated_color += texture_load(input_tx, texel + int2(x, -y)) * weight;
      accumulated_color += texture_load(input_tx, texel + int2(-x, -y)) * weight;
    }
  }

  imageStore(output_img, texel, accumulated_color);
}
