/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 input_size = texture_size(input_streak_tx);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. Similarly, transform the
   * vector into the sampler's space by dividing by the input size. */
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(input_size);
  vec2 vector = streak_vector / vec2(input_size);

  /* Load three equally spaced neighbors to the current pixel in the direction of the streak
   * vector. */
  vec4 neighbours[3];
  neighbours[0] = texture(input_streak_tx, coordinates + vector);
  neighbours[1] = texture(input_streak_tx, coordinates + vector * 2.0);
  neighbours[2] = texture(input_streak_tx, coordinates + vector * 3.0);

  /* Attenuate the value of two of the channels for each of the neighbors by multiplying by the
   * color modulator. The particular channels for each neighbor were chosen to be visually similar
   * to the modulation pattern of chromatic aberration. */
  neighbours[0].gb *= color_modulator;
  neighbours[1].rg *= color_modulator;
  neighbours[2].rb *= color_modulator;

  /* Compute the weighted sum of all neighbors using the given fade factors as weights. The
   * weights are expected to be lower for neighbors that are further away. */
  vec4 weighted_neighbours_sum = vec4(0.0);
  for (int i = 0; i < 3; i++) {
    weighted_neighbours_sum += fade_factors[i] * neighbours[i];
  }

  /* The output is the average between the center color and the weighted sum of the neighbors.
   * Which intuitively mean that highlights will spread in the direction of the streak, which is
   * the desired result. */
  vec4 center_color = texture(input_streak_tx, coordinates);
  vec4 output_color = (center_color + weighted_neighbours_sum) / 2.0;
  imageStore(output_streak_img, texel, output_color);
}
