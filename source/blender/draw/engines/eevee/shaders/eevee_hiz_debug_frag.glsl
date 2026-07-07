/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Debug hiz down sampling pass.
 * Output red if above any max pixels, blue otherwise.
 */

#include "infos/eevee_hiz_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(eevee_hiz_debug)

void main()
{
  int2 texel = int2(gl_FragCoord.xy);

  float depth0 = texelFetch(hiz_tx, texel, 0).r;

  float4 color = float4(0.1f, 0.1f, 1.0f, 1.0f);
  for (int i = 1; i < HIZ_MIP_COUNT; i++) {
    int2 lvl_texel = texel / int2(uint2(1) << uint(i));
    lvl_texel = min(lvl_texel, textureSize(hiz_tx, i) - 1);
    if (texelFetch(hiz_tx, lvl_texel, i).r < depth0) {
      color = float4(1.0f, 0.1f, 0.1f, 1.0f);
      break;
    }
  }
  out_debug_color_add = float4(color.rgb, 0.0f) * 0.2f;
  out_debug_color_mul = color;
}
