/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel. */
  vec2 coordinates = vec2(texel) + vec2(0.5);

  /* Transform the input image by transforming the domain coordinates with the inverse of input
   * image's transformation. The inverse transformation is an affine matrix and thus the
   * coordinates should be in homogeneous coordinates. */
  coordinates = (to_float3x3(inverse_transformation) * vec3(coordinates, 1.0)).xy;

  /* Subtract the offset and divide by the input image size to get the relevant coordinates into
   * the sampler's expected [0, 1] range. */
  vec2 normalized_coordinates = coordinates / vec2(texture_size(input_tx));

  imageStore(domain_img, texel, SAMPLER_FUNCTION(input_tx, normalized_coordinates));
}
