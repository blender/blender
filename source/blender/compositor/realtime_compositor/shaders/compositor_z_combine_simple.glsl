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

  /* Choose the closer pixel as the foreground, that is, the pixel with the lower z value. If Use
   * Alpha is disabled, return the foreground, otherwise, mix between the foreground and background
   * using the alpha of the foreground. */
  vec4 foreground_color = first_z_value < second_z_value ? first_color : second_color;
  vec4 background_color = first_z_value < second_z_value ? second_color : first_color;
  float mix_factor = use_alpha ? foreground_color.a : 1.0;
  vec4 combined_color = mix(background_color, foreground_color, mix_factor);

  /* Use the more opaque alpha from the two images. */
  combined_color.a = use_alpha ? max(second_color.a, first_color.a) : combined_color.a;

  float combined_z = min(first_z_value, second_z_value);

  imageStore(combined_img, texel, combined_color);
  imageStore(combined_z_img, texel, vec4(combined_z));
}
