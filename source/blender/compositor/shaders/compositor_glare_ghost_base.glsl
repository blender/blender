/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 input_size = texture_size(small_ghost_tx);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. */
  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(input_size);

  /* The small ghost is scaled down with the origin as the center of the image by a factor
   * of 2.13, while the big ghost is flipped and scaled up with the origin as the center of the
   * image by a factor of 0.97. Note that 1) The negative scale implements the flipping. 2)
   * Factors larger than 1 actually scales down the image since the factor multiplies the
   * coordinates and not the images itself. 3) The values are arbitrarily chosen using visual
   * judgment. */
  float small_ghost_scale = 2.13f;
  float big_ghost_scale = -0.97f;

  /* Scale the coordinates for the small and big ghosts, pre subtract 0.5 and post add 0.5 to use
   * 0.5 as the origin of the scaling. Notice that the big ghost is flipped due to the negative
   * scale. */
  float2 small_ghost_coordinates = (coordinates - 0.5f) * small_ghost_scale + 0.5f;
  float2 big_ghost_coordinates = (coordinates - 0.5f) * big_ghost_scale + 0.5f;

  /* The values of the ghosts are attenuated by the inverse distance to the center, such that they
   * are maximum at the center and become zero further from the center, making sure to take the
   * aforementioned scale into account. */
  float distance_to_center = distance(coordinates, float2(0.5f)) * 2.0f;
  float small_ghost_attenuator = max(0.0f, 1.0f - distance_to_center * small_ghost_scale);
  float big_ghost_attenuator = max(0.0f, 1.0f - distance_to_center * abs(big_ghost_scale));

  float4 small_ghost = texture(small_ghost_tx, small_ghost_coordinates) * small_ghost_attenuator;
  float4 big_ghost = texture(big_ghost_tx, big_ghost_coordinates) * big_ghost_attenuator;

  imageStore(combined_ghost_img, texel, small_ghost + big_ghost);
}
