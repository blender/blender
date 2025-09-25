/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308f) {
    return (c < 0.0f) ? 0.0f : c * 12.92f;
  }
  else {
    return 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
  }
}

float4 texture_read_as_linearrgb(sampler2D tex, bool premultiplied, float2 co)
{
  /* By convention image textures return scene linear colors, but
   * overlays still assume srgb. */
  float4 col = texture(tex, co);
  /* Un-pre-multiply if stored multiplied, since straight alpha is expected by shaders. */
  if (premultiplied && !(col.a == 0.0f || col.a == 1.0f)) {
    col.rgb = col.rgb / col.a;
  }
  return col;
}

float4 texture_read_as_srgb(sampler2D tex, bool premultiplied, float2 co)
{
  float4 col = texture_read_as_linearrgb(tex, premultiplied, co);
  col.r = linearrgb_to_srgb(col.r);
  col.g = linearrgb_to_srgb(col.g);
  col.b = linearrgb_to_srgb(col.b);
  return col;
}
