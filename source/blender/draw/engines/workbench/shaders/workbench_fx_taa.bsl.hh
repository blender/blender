/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_fullscreen.bsl.hh"

namespace workbench::taa {

[[vertex]]
void vert([[vertex_id]] const int vert_id, [[position]] float4 &position)
{
  fullscreen_vertex(vert_id, position);
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

struct TemporalAA {
  [[push_constant]] float samplesWeights[9];

  [[sampler(0)]] sampler2D color_buffer;
};

[[fragment]]
void frag([[resource_table]] const TemporalAA &srt,
          [[frag_coord]] const float4 frag_co,
          [[out]] FragOut &frag_out)
{
  float2 texel_size = 1.0f / float2(textureSize(srt.color_buffer, 0));
  float2 uv = frag_co.xy * texel_size;

  frag_out.color = float4(0.0f);
  int i = 0;
  for (int x = -1; x <= 1; x++) [[unroll]] {
    for (int y = -1; y <= 1; y++) [[unroll]] {
      float4 color = texture(srt.color_buffer, uv + float2(x, y) * texel_size);
      /* Clamp infinite inputs (See #112211). */
      color = clamp(color, float4(0.0f), float4(1e10f));
      /* Use log2 space to avoid highlights creating too much aliasing. */
      color.rgb = log2(color.rgb + 1.0f);

      frag_out.color += color * srt.samplesWeights[i];
      i++;
    }
  }
}

}  // namespace workbench::taa

PipelineGraphic workbench_taa(workbench::taa::vert, workbench::taa::frag);
