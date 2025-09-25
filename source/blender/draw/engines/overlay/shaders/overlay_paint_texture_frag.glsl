/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_paint_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_paint_texture)

#include "draw_colormanagement_lib.glsl"

void main()
{
  float4 mask = float4(texture_read_as_srgb(mask_image, mask_image_premultiplied, uv_interp).rgb,
                       1.0f);
  if (mask_invert_stencil) {
    mask.rgb = 1.0f - mask.rgb;
  }
  float mask_step = smoothstep(0.0f, 3.0f, mask.r + mask.g + mask.b);
  mask.rgb *= mask_color;
  mask.a = mask_step * opacity;

  frag_color = mask;
}
