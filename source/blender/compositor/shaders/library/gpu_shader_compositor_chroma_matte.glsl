/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"

/* Algorithm from the book Video Demystified. Chapter 7. Chroma Keying. */
void node_composite_chroma_matte(float4 color,
                                 float4 key,
                                 float minimum,
                                 float maximum,
                                 float falloff,
                                 out float4 result,
                                 out float matte)
{
  float4 color_ycca;
  rgba_to_ycca_itu_709(color, color_ycca);
  float4 key_ycca;
  rgba_to_ycca_itu_709(key, key_ycca);

  /* Normalize the CrCb components into the [-1, 1] range. */
  float2 color_cc = color_ycca.yz * 2.0f - 1.0f;
  float2 key_cc = normalize(key_ycca.yz * 2.0f - 1.0f);

  /* Rotate the color onto the space of the key such that x axis of the color space passes through
   * the key color. */
  color_cc = from_direction(key_cc * float2(1.0f, -1.0f)) * color_cc;

  /* Compute foreground key. If positive, the value is in the [0, 1] range. */
  float foreground_key = color_cc.x - (abs(color_cc.y) / tan(maximum / 2.0f));

  /* Negative foreground key values retain the original alpha. Positive values are scaled by the
   * falloff, while colors that make an angle less than the minimum angle get a zero alpha. */
  float alpha = color.a;
  if (foreground_key > 0.0f) {
    alpha = 1.0f - (foreground_key / falloff);

    if (abs(atan(color_cc.y, color_cc.x)) < (minimum / 2.0f)) {
      alpha = 0.0f;
    }
  }

  /* Compute output. */
  matte = min(alpha, color.a);
  result = color * matte;
}
