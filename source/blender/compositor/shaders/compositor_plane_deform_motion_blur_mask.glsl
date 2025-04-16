/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  int2 texel = int2(gl_GlobalInvocationID.xy);

  float2 coordinates = (float2(texel) + float2(0.5f)) / float2(imageSize(mask_img));

  float accumulated_mask = 0.0f;
  for (int i = 0; i < number_of_motion_blur_samples; i++) {
    float3x3 homography_matrix = to_float3x3(homography_matrices[i]);

    float3 transformed_coordinates = homography_matrix * float3(coordinates, 1.0f);
    /* Point is at infinity and will be zero when sampled, so early exit. */
    if (transformed_coordinates.z == 0.0f) {
      continue;
    }
    float2 projected_coordinates = transformed_coordinates.xy / transformed_coordinates.z;

    bool is_inside_plane = all(greaterThanEqual(projected_coordinates, float2(0.0f))) &&
                           all(lessThanEqual(projected_coordinates, float2(1.0f)));
    accumulated_mask += is_inside_plane ? 1.0f : 0.0f;
  }

  accumulated_mask /= number_of_motion_blur_samples;

  imageStore(mask_img, texel, float4(accumulated_mask));
}
