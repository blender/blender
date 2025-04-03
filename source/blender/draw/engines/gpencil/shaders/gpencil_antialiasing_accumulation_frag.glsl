/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_info.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_antialiasing_accumulation)

vec4 colorspace_scene_to_perceptual(vec4 color)
{
  return vec4(log2(color.rgb + 0.5), color.a);
}

vec4 colorspace_perceptual_to_scene(vec4 color)
{
  return vec4(exp2(color.rgb) - 0.5, color.a);
}

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec4 data_src = colorspace_scene_to_perceptual(max(vec4(0.0), imageLoadFast(src_img, texel)));
  vec4 data_dst = colorspace_scene_to_perceptual(max(vec4(0.0), imageLoadFast(dst_img, texel)));
  vec4 result = data_src * weight_src;
  if (weight_dst > 0.0) {
    /* Avoid uncleared data to mess with the result value. */
    result += data_dst * weight_dst;
  }
  if (data_src.a == 1.0 && data_dst.a == 1.0) {
    /* Avoid float imprecision leading to non fully opaque renders. */
    result.a = 1.0;
  }
  imageStoreFast(dst_img, texel, colorspace_perceptual_to_scene(result));
}
