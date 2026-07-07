/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/compositor_plane_deform_infos.hh"

COMPUTE_SHADER_CREATE_INFO(compositor_plane_deform_mask)

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(imageSize(mask_img));

  float3 transformed_coordinates = to_float3x3(homography_matrix) * float3(coordinates, 1.0f);
  /* Point is at infinity and will be zero when sampled, so early exit. */
  if (transformed_coordinates.z == 0.0f) {
    imageStore(mask_img, texel, float4(0.0f));
    return;
  }
  float2 projected_coordinates = transformed_coordinates.xy / transformed_coordinates.z;

  bool is_inside_plane_x = projected_coordinates.x >= 0.0f && projected_coordinates.x <= 1.0f;
  bool is_inside_plane_y = projected_coordinates.y >= 0.0f && projected_coordinates.y <= 1.0f;

  bool is_x_masked = is_inside_plane_x || !is_x_clipped;
  bool is_y_masked = is_inside_plane_y || !is_y_clipped;

  float mask_value = is_x_masked && is_y_masked ? 1.0f : 0.0f;

  imageStore(mask_img, texel, float4(mask_value));
}
