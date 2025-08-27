/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "vk_backbuffer_blit_info.hh"

COMPUTE_SHADER_CREATE_INFO(vk_backbuffer_blit)

void main()
{
  ivec2 dst_texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 src_size = ivec2(imageSize(src_img));
  ivec2 src_texel = ivec2(dst_texel.x, src_size.y - dst_texel.y - 1);
  vec4 color = imageLoad(src_img, ivec2(src_texel));
  /*
   * Convert from extended sRGB non-linear to linear.
   *
   * Preserves negative wide gamut values with sign/abs.
   * Gamma 2.2 is used instead of the sRGB piecewise transfer function, because
   * most SDR sRGB displays decode with gamma 2.2, and that's what we are trying
   * to match.
   */
  color.rgb = sign(color.rgb) * pow(abs(color.rgb), vec3(2.2f)) * sdr_scale;
  imageStore(dst_img, ivec2(dst_texel), color);
}
