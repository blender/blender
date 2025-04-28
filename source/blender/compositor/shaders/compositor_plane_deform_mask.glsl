/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  bool is_inside_plane = all(greaterThanEqual(projected_coordinates, float2(0.0f))) &&
                         all(lessThanEqual(projected_coordinates, float2(1.0f)));
  float mask_value = is_inside_plane ? 1.0f : 0.0f;

  imageStore(mask_img, texel, float4(mask_value));
}
