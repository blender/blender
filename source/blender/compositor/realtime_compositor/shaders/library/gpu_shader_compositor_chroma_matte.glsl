/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(gpu_shader_common_math_utils.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

/* Algorithm from the book Video Demystified. Chapter 7. Chroma Keying. */
void node_composite_chroma_matte(vec4 color,
                                 vec4 key,
                                 float acceptance,
                                 float cutoff,
                                 float falloff,
                                 out vec4 result,
                                 out float matte)
{
  vec4 color_ycca;
  rgba_to_ycca_itu_709(color, color_ycca);
  vec4 key_ycca;
  rgba_to_ycca_itu_709(key, key_ycca);

  /* Normalize the CrCb components into the [-1, 1] range. */
  vec2 color_cc = color_ycca.yz * 2.0 - 1.0;
  vec2 key_cc = key_ycca.yz * 2.0 - 1.0;

  /* Rotate the color onto the space of the key such that x axis of the color space passes through
   * the key color. */
  color_cc = vector_to_rotation_matrix(key_cc * vec2(1.0, -1.0)) * color_cc;

  /* Compute foreground key. If positive, the value is in the [0, 1] range. */
  float foreground_key = color_cc.x - (abs(color_cc.y) / acceptance);

  /* Negative foreground key values retain the original alpha. Positive values are scaled by the
   * falloff, while colors that make an angle less than the cutoff angle get a zero alpha. */
  float alpha = color.a;
  if (foreground_key > 0.0) {
    alpha = 1.0 - (foreground_key / falloff);

    if (abs(atan(color_cc.y, color_cc.x)) < (cutoff / 2.0)) {
      alpha = 0.0;
    }
  }

  /* Compute output. */
  matte = min(alpha, color.a);
  result = color * matte;
}
