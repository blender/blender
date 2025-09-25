/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_gpencil_stroke_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_gpencil_stroke)

void main()
{
  constexpr float2 center = float2(0, 0.5f);
  float4 tColor = interp.mColor;
  /* if alpha < 0, then encap */
  if (tColor.a < 0) {
    tColor.a = tColor.a * -1.0f;
    float dist = length(interp.mTexCoord - center);
    if (dist > 0.25f) {
      gpu_discard_fragment();
    }
  }
  /* Solid */
  fragColor = tColor;
}
