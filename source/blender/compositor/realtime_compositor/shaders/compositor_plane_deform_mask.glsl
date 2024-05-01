/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(imageSize(mask_img));

  vec3 transformed_coordinates = mat3(homography_matrix) * vec3(coordinates, 1.0);
  vec2 projected_coordinates = transformed_coordinates.xy / transformed_coordinates.z;

  bool is_inside_plane = all(greaterThanEqual(projected_coordinates, vec2(0.0))) &&
                         all(lessThanEqual(projected_coordinates, vec2(1.0)));
  float mask_value = is_inside_plane ? 1.0 : 0.0;

  imageStore(mask_img, texel, vec4(mask_value));
}
