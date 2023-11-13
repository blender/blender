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
  float mask_value = texture_load(mask_tx, texel).x;

  vec4 combined_color = mix(second_color, first_color, mask_value);
  /* Use the more opaque alpha from the two images. */
  combined_color.a = use_alpha ? max(second_color.a, first_color.a) : combined_color.a;

  float combined_z = mix(second_z_value, first_z_value, mask_value);

  imageStore(combined_img, texel, combined_color);
  imageStore(combined_z_img, texel, vec4(combined_z));
}
