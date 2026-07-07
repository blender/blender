/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_antialiasing_accumulation)

float4 colorspace_scene_to_perceptual(float4 color)
{
  return float4(log2(color.rgb + 0.5f), color.a);
}

float4 colorspace_perceptual_to_scene(float4 color)
{
  return float4(exp2(color.rgb) - 0.5f, color.a);
}

void main()
{
  int2 texel = int2(gl_FragCoord.xy);
  float4 data_src = colorspace_scene_to_perceptual(
      max(float4(0.0f), imageLoadFast(src_img, texel)));
  float4 data_dst = colorspace_scene_to_perceptual(
      max(float4(0.0f), imageLoadFast(dst_img, texel)));
  float4 result = data_src * weight_src;
  if (weight_dst > 0.0f) {
    /* Avoid uncleared data to mess with the result value. */
    result += data_dst * weight_dst;
  }
  if (data_src.a == 1.0f && data_dst.a == 1.0f) {
    /* Avoid float imprecision leading to non fully opaque renders. */
    result.a = 1.0f;
  }
  imageStoreFast(dst_img, texel, colorspace_perceptual_to_scene(result));
}
