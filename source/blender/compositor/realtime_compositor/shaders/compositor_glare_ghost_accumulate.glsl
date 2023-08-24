/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 input_size = texture_size(input_ghost_tx);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. */
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(input_size);

  /* We accumulate four variants of the input ghost texture, each is scaled by some amount and
   * possibly multiplied by some color as a form of color modulation. */
  vec4 accumulated_ghost = vec4(0.0);
  for (int i = 0; i < 4; i++) {
    float scale = scales[i];
    vec4 color_modulator = color_modulators[i];

    /* Scale the coordinates for the ghost, pre subtract 0.5 and post add 0.5 to use 0.5 as the
     * origin of the scaling. */
    vec2 scaled_coordinates = (coordinates - 0.5) * scale + 0.5;

    /* The value of the ghost is attenuated by a scalar multiple of the inverse distance to the
     * center, such that it is maximum at the center and become zero further from the center,
     * making sure to take the scale into account. The scaler multiple of 1 / 4 is chosen using
     * visual judgement. */
    float distance_to_center = distance(coordinates, vec2(0.5)) * 2.0;
    float attenuator = max(0.0, 1.0 - distance_to_center * abs(scale)) / 4.0;

    /* Accumulate the scaled ghost after attenuating and color modulating its value. */
    vec4 multiplier = attenuator * color_modulator;
    accumulated_ghost += texture(input_ghost_tx, scaled_coordinates) * multiplier;
  }

  vec4 current_accumulated_ghost = imageLoad(accumulated_ghost_img, texel);
  imageStore(accumulated_ghost_img, texel, current_accumulated_ghost + accumulated_ghost);
}
