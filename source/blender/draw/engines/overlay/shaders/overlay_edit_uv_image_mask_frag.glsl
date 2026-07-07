/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_edit_uv_mask_image)

#include "draw_colormanagement_lib.glsl"

void main()
{
  float2 uvs_clamped = clamp(uvs, 0.0f, 1.0f);
  float mask_value = texture_read_as_linearrgb(img_tx, true, uvs_clamped).r;
  mask_value = mix(1.0f, mask_value, opacity);
  frag_color = float4(color.rgb * mask_value, color.a);
}
