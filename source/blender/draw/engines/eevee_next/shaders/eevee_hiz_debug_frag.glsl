/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Debug hiz down sampling pass.
 * Output red if above any max pixels, blue otherwise.
 */

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);

  float depth0 = texelFetch(hiz_tx, texel, 0).r;

  vec4 color = vec4(0.1, 0.1, 1.0, 1.0);
  for (int i = 1; i < HIZ_MIP_COUNT; i++) {
    ivec2 lvl_texel = texel / ivec2(uvec2(1) << uint(i));
    lvl_texel = min(lvl_texel, textureSize(hiz_tx, i) - 1);
    if (texelFetch(hiz_tx, lvl_texel, i).r < depth0) {
      color = vec4(1.0, 0.1, 0.1, 1.0);
      break;
    }
  }
  out_debug_color_add = vec4(color.rgb, 0.0) * 0.2;
  out_debug_color_mul = color;
}
