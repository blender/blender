/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_bicubic_sampler_lib.glsl"
#include "gpu_shader_compositor_texture_utilities.glsl"

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);
  float2 output_size = float2(imageSize(output_img));

  float2 coordinates = (float2(texel) + float2(0.5f)) / output_size;

  float3 transformed_coordinates = to_float3x3(homography_matrix) * float3(coordinates, 1.0f);
  /* Point is at infinity and will be zero when sampled, so early exit. */
  if (transformed_coordinates.z == 0.0f) {
    imageStore(output_img, texel, float4(0.0f));
    return;
  }
  float2 projected_coordinates = transformed_coordinates.xy / transformed_coordinates.z;

  float4 sampled_color = SAMPLER_FUNCTION(input_tx, projected_coordinates);

  /* Premultiply the mask value as an alpha. */
  float4 plane_color = sampled_color * texture_load(mask_tx, texel).x;

  imageStore(output_img, texel, plane_color);
}
