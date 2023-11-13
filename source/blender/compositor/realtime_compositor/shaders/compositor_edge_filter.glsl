/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  /* Compute the dot product between the 3x3 window around the pixel and the edge detection kernel
   * in the X direction and Y direction. The Y direction kernel is computed by transposing the
   * given X direction kernel. */
  vec3 color_x = vec3(0);
  vec3 color_y = vec3(0);
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      vec3 color = texture_load(input_tx, texel + ivec2(i - 1, j - 1)).rgb;
      color_x += color * ukernel[j][i];
      color_y += color * ukernel[i][j];
    }
  }

  /* Compute the channel-wise magnitude of the 2D vector composed from the X and Y edge detection
   * filter results. */
  vec3 magnitude = sqrt(color_x * color_x + color_y * color_y);

  /* Mix the channel-wise magnitude with the original color at the center of the kernel using the
   * input factor. */
  vec4 color = texture_load(input_tx, texel);
  magnitude = mix(color.rgb, magnitude, texture_load(factor_tx, texel).x);

  /* Store the channel-wise magnitude with the original alpha of the input. */
  imageStore(output_img, texel, vec4(magnitude, color.a));
}
