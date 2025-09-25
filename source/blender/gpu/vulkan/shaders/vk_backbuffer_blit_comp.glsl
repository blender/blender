/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "vk_backbuffer_blit_infos.hh"

COMPUTE_SHADER_CREATE_INFO(vk_backbuffer_blit)

float srgb_to_linearrgb(float c)
{
  if (c < 0.04045f) {
    return (c < 0.0f) ? 0.0f : c * (1.0f / 12.92f);
  }

  return pow((c + 0.055f) * (1.0f / 1.055f), 2.4f);
}

vec3 nonlinear_to_linear_scrgb(vec3 c)
{
#ifdef USE_GAMMA22
  return pow(c, vec3(2.2f));
#else
  return vec3(srgb_to_linearrgb(c.r), srgb_to_linearrgb(c.g), srgb_to_linearrgb(c.b));
#endif
}

void main()
{
  ivec2 dst_texel = ivec2(gl_GlobalInvocationID.xy);
  ivec2 src_size = ivec2(imageSize(src_img));
  ivec2 src_texel = ivec2(dst_texel.x, src_size.y - dst_texel.y - 1);
  vec4 color = imageLoad(src_img, ivec2(src_texel));
  /*
   * Convert from extended sRGB non-linear to linear.
   *
   * Preserves negative wide gamut values with sign/abs. May use either gamma 2.2
   * decode to match most SDR sRGB displays, or the piecewise sRGB function to
   * match Windows SDR applications in HDR node.
   */
  color.rgb = sign(color.rgb) * nonlinear_to_linear_scrgb(abs(color.rgb)) * sdr_scale;
  imageStore(dst_img, ivec2(dst_texel), color);
}
