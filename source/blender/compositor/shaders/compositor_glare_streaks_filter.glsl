/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 input_size = texture_size(input_streak_tx);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. Similarly, transform the
   * vector into the sampler's space by dividing by the input size. */
  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(input_size);
  float2 vector = streak_vector / float2(input_size);

  /* Load three equally spaced neighbors to the current pixel in the direction of the streak
   * vector. */
  float4 neighbors[3];
  neighbors[0] = texture(input_streak_tx, coordinates + vector);
  neighbors[1] = texture(input_streak_tx, coordinates + vector * 2.0f);
  neighbors[2] = texture(input_streak_tx, coordinates + vector * 3.0f);

  /* Attenuate the value of two of the channels for each of the neighbors by multiplying by the
   * color modulator. The particular channels for each neighbor were chosen to be visually similar
   * to the modulation pattern of chromatic aberration. */
  neighbors[0].gb *= color_modulator;
  neighbors[1].rg *= color_modulator;
  neighbors[2].rb *= color_modulator;

  /* Compute the weighted sum of all neighbors using the given fade factors as weights. The
   * weights are expected to be lower for neighbors that are further away. */
  float4 weighted_neighbors_sum = float4(0.0f);
  for (int i = 0; i < 3; i++) {
    weighted_neighbors_sum += fade_factors[i] * neighbors[i];
  }

  /* The output is the average between the center color and the weighted sum of the neighbors.
   * Which intuitively mean that highlights will spread in the direction of the streak, which is
   * the desired result. */
  float4 center_color = texture(input_streak_tx, coordinates);
  float4 output_color = (center_color + weighted_neighbors_sum) / 2.0f;
  imageStore(output_streak_img, texel, output_color);
}
