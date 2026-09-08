/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

namespace builtin {

struct ColorSpace {
  [[push_constant]] const bool srgbTarget;

  /**
   * Input is Rec.709 sRGB.
   * Output is Rec.709 linear if hardware will add a Linear to sRGB conversion, NOOP otherwise.
   *
   * As per GPU API design, all frame-buffers with SRGBA_8_8_8_8 attachments will always enable
   * SRGB rendering. In this mode the shader output is expected to be in a linear color space. This
   * allows to do the blending stage with linear values (more correct) and then store the result in
   * 8bpc keeping accurate colors.
   *
   * To ensure consistent result (blending excluded) between a frame-buffer using SRGBA_8_8_8_8 and
   * one using RGBA_8_8_8_8, we need to do the sRGB > linear conversion to counteract the hardware
   * encoding during frame-buffer output.
   *
   * For reference:  https://wikis.khronos.org/opengl/framebuffer#Colorspace
   */
  float4 rec709_srgb_to_output_space(float4 srgb_color) const
  {
    /**
     * IMPORTANT: srgbTarget denote that the output is expected to be in __linear__ space.
     * https://wikis.khronos.org/opengl/framebuffer#Colorspace
     */
    if (!srgbTarget) {
      /* Input should already be in sRGB. */
      return srgb_color;
    }
    /* Note that this is simply counteracting the hardware Linear > sRGB conversion. */
    float3 c = max(srgb_color.rgb, float3(0.0f));
    float3 c1 = c * (1.0f / 12.92f);
    float3 c2 = pow((c + 0.055f) * (1.0f / 1.055f), float3(2.4f));
    float4 linear_color;
    linear_color.rgb = mix(c1, c2, step(float3(0.04045f), c));
    linear_color.a = srgb_color.a;
    return linear_color;
  }

  /* Input is Blender Scene Linear. Output is Rec.709 sRGB. */
  static float4 scene_linear_to_rec709_srgb(float3x3 scene_linear_to_rec709,
                                            float4 scene_linear_color)
  {
    float3 rec709_linear = scene_linear_to_rec709 * scene_linear_color.rgb;

    /* TODO(fclem): For wide gamut (extended sRGB), we need to encode negative values in a certain
     * way here. */

    /* Linear to sRGB transform. */
    float3 c = max(rec709_linear, float3(0.0f));
    float3 c1 = c * 12.92f;
    float3 c2 = 1.055f * pow(c, float3(1.0f / 2.4f)) - 0.055f;
    float4 srgb_color;
    srgb_color.rgb = mix(c1, c2, step(float3(0.0031308f), c));
    srgb_color.a = scene_linear_color.a;
    return srgb_color;
  }
};

}  // namespace builtin
