/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_desaturate_color_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_2D_image_desaturate_color)

void main()
{
  float4 tex = texture(image, texCoord_interp);
  tex.rgb = ((0.3333333f * factor) * float3(tex.r + tex.g + tex.b)) + (tex.rgb * (1.0f - factor));
  fragColor = tex * color;
}
