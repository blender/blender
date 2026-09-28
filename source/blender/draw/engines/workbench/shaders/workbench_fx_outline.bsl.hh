/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_fullscreen.bsl.hh"
#include "workbench_shader_shared.hh"

namespace workbench::outline {

[[vertex]]
void vert([[vertex_id]] const int vert_id, [[position]] float4 &position)
{
  fullscreen_vertex(vert_id, position);
}

struct Outlines {
  [[sampler(0)]] usampler2D object_id_buffer;
  [[uniform(WB_WORLD_SLOT)]] WorldData &world_data;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]]
void frag([[resource_table]] const Outlines &srt,
          [[frag_coord]] const float4 frag_co,
          [[out]] FragOut &frag_out)
{
  float2 uv = frag_co.xy / float2(textureSize(srt.object_id_buffer, 0));

  float3 offset = float3(srt.world_data.viewport_size_inv, 0.0f) * srt.world_data.ui_scale;

  uint center_id = texture(srt.object_id_buffer, uv).r;
  uint4 adjacent_ids = uint4(texture(srt.object_id_buffer, uv + offset.zy).r,
                             texture(srt.object_id_buffer, uv - offset.zy).r,
                             texture(srt.object_id_buffer, uv + offset.xz).r,
                             texture(srt.object_id_buffer, uv - offset.xz).r);

  float outline_opacity = 1.0f - dot(float4(equal(uint4(center_id), adjacent_ids)), float4(0.25f));

  frag_out.color = srt.world_data.object_outline_color * outline_opacity;
}

}  // namespace workbench::outline

PipelineGraphic workbench_effect_outline(workbench::outline::vert, workbench::outline::frag);
