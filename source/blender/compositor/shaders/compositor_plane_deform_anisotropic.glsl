/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  /* The derivatives of the projected coordinates with respect to x and y are the first and
   * second columns respectively, divided by the z projection factor as can be shown by
   * differentiating the above matrix multiplication with respect to x and y. Divide by the
   * output size since textureGrad assumes derivatives with respect to texel coordinates. */
  float2 x_gradient = (homography_matrix[0].xy / transformed_coordinates.z) / output_size.x;
  float2 y_gradient = (homography_matrix[1].xy / transformed_coordinates.z) / output_size.y;

  float4 sampled_color = textureGrad(input_tx, projected_coordinates, x_gradient, y_gradient);

  /* Premultiply the mask value as an alpha. */
  float4 plane_color = sampled_color * texture_load(mask_tx, texel).x;

  imageStore(output_img, texel, plane_color);
}
