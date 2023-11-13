/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 first_color = texture_load(first_tx, texel);
  vec4 second_color = texture_load(second_tx, texel);
  float first_z_value = texture_load(first_z_tx, texel).x;
  float second_z_value = texture_load(second_z_tx, texel).x;

  /* Mix between the first and second images using a mask such that the image with the object
   * closer to the camera is returned. The mask value is then 1, and thus returns the first image
   * if its Z value is less than that of the second image. Otherwise, its value is 0, and thus
   * returns the second image. Furthermore, if the object in the first image is closer but has a
   * non-opaque alpha, then the alpha is used as a mask, but only if Use Alpha is enabled. */
  float z_combine_factor = float(first_z_value < second_z_value);
  float alpha_factor = use_alpha ? first_color.a : 1.0;
  float mix_factor = z_combine_factor * alpha_factor;

  vec4 combined_color = mix(second_color, first_color, mix_factor);
  /* Use the more opaque alpha from the two images. */
  combined_color.a = use_alpha ? max(second_color.a, first_color.a) : combined_color.a;

  float combined_z = mix(second_z_value, first_z_value, mix_factor);

  imageStore(combined_img, texel, combined_color);
  imageStore(combined_z_img, texel, vec4(combined_z));
}
