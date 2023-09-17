/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void render_pass_color_out(int pass_index, vec3 color)
{
  if (pass_index >= 0) {
    imageStore(rp_color_img, ivec3(ivec2(gl_FragCoord.xy), pass_index), vec4(color, 0.0));
  }
}

void render_pass_value_out(int pass_index, float value)
{
  if (pass_index >= 0) {
    imageStore(rp_value_img, ivec3(ivec2(gl_FragCoord.xy), pass_index), vec4(value));
  }
}