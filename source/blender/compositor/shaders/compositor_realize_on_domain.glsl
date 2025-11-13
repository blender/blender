/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  const int2 texel = int2(gl_GlobalInvocationID.xy);

  const float2 texel_coordinates = float2(texel) + float2(0.5f);

  /* Transform the input image by transforming the domain coordinates with the inverse of input
   * image's transformation. The inverse transformation is an affine matrix and thus the
   * coordinates should be in homogeneous coordinates. */
  const float2 transformed_coordinates =
      (to_float3x3(inverse_transformation) * float3(texel_coordinates, 1.0f)).xy;

  const float2 normalized_coordinates = transformed_coordinates / float2(texture_size(input_tx));
  imageStore(domain_img, texel, SAMPLER_FUNCTION(input_tx, normalized_coordinates));
}
