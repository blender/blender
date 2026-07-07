/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_image_overlays_merge_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_cycles_display_fallback)

float2 normalize_coordinates()
{
  return (float2(2.0f) * (pos / fullscreen)) - float2(1.0f);
}

void main()
{
  gl_Position = float4(normalize_coordinates(), 0.0f, 1.0f);
  texCoord_interp = texCoord;
}
