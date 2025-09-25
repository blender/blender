/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  int2 input_size = texture_size(input_tx);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. */
  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(input_size);

  /* Note that the input displacement is in pixel space, so divide by the input size to transform
   * it into the normalized sampler space. */
  float2 displacement = texture_load(displacement_tx, texel).xy / float2(input_size);
  float2 displaced_coordinates = coordinates - displacement;

  imageStore(output_img, texel, SAMPLER_FUNCTION(input_tx, displaced_coordinates));
}
