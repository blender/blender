/* SPDX-FileCopyrightText: 2020-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Overlay X-Ray fade.
 *
 * Adds a low-opacity fade behind scene geometry. This allows for a nice
 * transition between opaque, X-ray and wireframe modes, and is only
 * available if X-ray mode is enabled or we are in wireframe mode.
 */

#pragma once

#include "gpu_shader_compat.hh"
#include "gpu_shader_fullscreen_lib.glsl"
#include "infos/overlay_common_infos.hh"

namespace overlay::xray_fade {

struct TexelData {
  float depth;
  float xray_depth;
};

struct Resources {
  [[sampler(0)]] const sampler2DDepth depth_tx;
  [[sampler(1)]] const sampler2DDepth depth_in_front_tx;
  [[sampler(2)]] const sampler2DDepth xray_depth_tx;
  [[sampler(3)]] const sampler2DDepth xray_depth_in_front_tx;

  [[push_constant]] const float opacity;

  TexelData sample_texel(float2 uv)
  {
    return {
        .depth = textureLod(depth_tx, uv, 0.0f).r,
        .xray_depth = textureLod(xray_depth_tx, uv, 0.0f).r,
    };
  }

  TexelData sample_texel_in_front(float2 uv)
  {
    return {
        .depth = textureLod(depth_in_front_tx, uv, 0.0f).r,
        .xray_depth = textureLod(xray_depth_in_front_tx, uv, 0.0f).r,
    };
  }
};

struct VertOut {
  [[smooth]] float2 uv;
};

[[vertex]] void vert_main([[vertex_id]] const int &vert_id,
                          [[out]] VertOut &v_out,
                          [[position]] float4 &position)
{
  fullscreen_vertex(vert_id, position, v_out.uv);
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void frag_main([[frag_coord]] const float4 &frag_coord,
                            [[resource_table]] Resources &srt,
                            [[in]] const VertOut &v_in,
                            [[out]] FragOut &frag_out)
{
  TexelData data_in_front = srt.sample_texel_in_front(v_in.uv);

  if (data_in_front.xray_depth != 1.0f) {
    if (data_in_front.depth < data_in_front.xray_depth) {
      frag_out.color = float4(srt.opacity);
      return;
    }

    gpu_discard_fragment();
    return;
  }

  TexelData data = srt.sample_texel(v_in.uv);

  /* Merge in-front depth. */
  if (data_in_front.depth != 1.0f) {
    data.depth = 0.0f;
  }

  if (data.depth < data.xray_depth) {
    frag_out.color = float4(srt.opacity);
    return;
  }

  gpu_discard_fragment();
}

PipelineGraphic pipeline(vert_main, frag_main);

}  // namespace overlay::xray_fade
