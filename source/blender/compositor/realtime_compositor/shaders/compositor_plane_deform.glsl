/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image size
   * to get the coordinates into the sampler's expected [0, 1] range. */
  vec2 coordinates = (vec2(texel) + vec2(0.5)) / vec2(texture_size(input_tx));

  vec3 transformed_coordinates = mat3(homography_matrix) * vec3(coordinates, 1.0);
  vec2 projected_coordinates = transformed_coordinates.xy / transformed_coordinates.z;

  /* The derivatives of the projected coordinates with respect to x and y are the first and
   * second columns respectively, divided by the z projection factor as can be shown by
   * differentiating the above matrix multiplication with respect to x and y. */
  vec2 x_gradient = homography_matrix[0].xy / transformed_coordinates.z;
  vec2 y_gradient = homography_matrix[1].xy / transformed_coordinates.z;

  vec4 sampled_color = textureGrad(input_tx, projected_coordinates, x_gradient, y_gradient);

  imageStore(output_img, texel, sampled_color);
  imageStore(mask_img, texel, sampled_color.aaaa);
}
