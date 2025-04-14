/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 input_size = texture_size(input_ghost_tx);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. */
  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(input_size);

  /* We accumulate four variants of the input ghost texture, each is scaled by some amount and
   * possibly multiplied by some color as a form of color modulation. */
  float4 accumulated_ghost = float4(0.0f);
  for (int i = 0; i < 4; i++) {
    float scale = scales[i];
    float4 color_modulator = color_modulators[i];

    /* Scale the coordinates for the ghost, pre subtract 0.5 and post add 0.5 to use 0.5 as the
     * origin of the scaling. */
    float2 scaled_coordinates = (coordinates - 0.5f) * scale + 0.5f;

    /* The value of the ghost is attenuated by a scalar multiple of the inverse distance to the
     * center, such that it is maximum at the center and become zero further from the center,
     * making sure to take the scale into account. The scalar multiple of 1 / 4 is chosen using
     * visual judgment. */
    float distance_to_center = distance(coordinates, float2(0.5f)) * 2.0f;
    float attenuator = max(0.0f, 1.0f - distance_to_center * abs(scale)) / 4.0f;

    /* Accumulate the scaled ghost after attenuating and color modulating its value. */
    float4 multiplier = attenuator * color_modulator;
    accumulated_ghost += texture(input_ghost_tx, scaled_coordinates) * multiplier;
  }

  float4 current_accumulated_ghost = imageLoad(accumulated_ghost_img, texel);
  float4 combined_ghost = current_accumulated_ghost + accumulated_ghost;
  imageStore(accumulated_ghost_img, texel, float4(combined_ghost.rgb, 1.0f));
}
