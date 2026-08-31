/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

namespace builtin::display_fallback {

struct Resources {
  [[push_constant]] float2 fullscreen;
  [[sampler(0)]] const sampler2D image_texture;
};

struct VertIn {
  [[attribute(0)]] float2 pos;
  [[attribute(1)]] float2 texCoord;
};

struct VertOut {
  [[smooth]] float2 uv;
};

[[vertex]] void vert_main([[resource_table]] const Resources &srt,
                          [[position]] float4 &out_pos,
                          [[in]] const VertIn &v_in,
                          [[out]] VertOut &v_out)
{
  float2 ndc = (float2(2.0f) * (v_in.pos / srt.fullscreen)) - float2(1.0f);
  out_pos = float4(ndc, 0.0f, 1.0f);
  v_out.uv = v_in.texCoord;
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void frag_main([[resource_table]] const Resources &srt,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  frag_out.color = texture(srt.image_texture, v_out.uv);
}

}  // namespace builtin::display_fallback

PipelineGraphic gpu_shader_cycles_display_fallback(builtin::display_fallback::vert_main,
                                                   builtin::display_fallback::frag_main);
