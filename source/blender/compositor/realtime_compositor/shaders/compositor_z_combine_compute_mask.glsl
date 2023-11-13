/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 first_color = texture_load(first_tx, texel);
  float first_z_value = texture_load(first_z_tx, texel).x;
  float second_z_value = texture_load(second_z_tx, texel).x;

  /* The same logic as in compositor_z_combine_simple.glsl but only computes the mask to be later
   * anti-aliased and used for mixing, see the logic in that file for more information. */
  float z_combine_factor = float(first_z_value < second_z_value);
  float alpha_factor = use_alpha ? first_color.a : 1.0;
  float mix_factor = z_combine_factor * alpha_factor;

  imageStore(mask_img, texel, vec4(mix_factor));
}
