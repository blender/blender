/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_effect_outline_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(workbench_effect_outline)

void main()
{
  float3 offset = float3(world_data.viewport_size_inv, 0.0f) * world_data.ui_scale;
  float2 uv = screen_uv;

  uint center_id = texture(object_id_buffer, uv).r;
  uint4 adjacent_ids = uint4(texture(object_id_buffer, uv + offset.zy).r,
                             texture(object_id_buffer, uv - offset.zy).r,
                             texture(object_id_buffer, uv + offset.xz).r,
                             texture(object_id_buffer, uv - offset.xz).r);

  float outline_opacity = 1.0f - dot(float4(equal(uint4(center_id), adjacent_ids)), float4(0.25f));

  frag_color = world_data.object_outline_color * outline_opacity;
}
