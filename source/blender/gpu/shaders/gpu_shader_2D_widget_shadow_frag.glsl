/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_widget_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_widget_shadow)

void main()
{
  fragColor = float4(0.0f);
  /* Manual curve fit of the falloff curve of previous drawing method. */
  float shadow_alpha = alpha * (shadowFalloff * shadowFalloff * 0.722f + shadowFalloff * 0.277f);
  float inner_alpha = smoothstep(0.0f, 0.05f, innerMask);

  fragColor.a = inner_alpha * shadow_alpha;
}
