/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_common_color_utils.glsl"

namespace sequencer::zebra {

struct Resources {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[push_constant]] float zebra_limit;
  [[push_constant]] bool img_premultiplied;
  [[sampler(0)]] sampler2D image;
};

struct VertIn {
  [[attribute(0)]] float2 pos;
  [[attribute(1)]] float2 texCoord;
};

struct VertOut {
  [[smooth]] float2 uv;
};

[[vertex]] void main_vert([[vertex_id]] const int vert_id,
                          [[instance_id]] const int inst_id,
                          [[resource_table]] Resources &srt,
                          [[in]] const VertIn &v_in,
                          [[out]] VertOut &v_out,
                          [[position]] float4 &position)
{
  position = srt.ModelViewProjectionMatrix * float4(v_in.pos.xy, 0.0f, 1.0f);
  v_out.uv = v_in.texCoord;
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void main_frag([[resource_table]] const Resources &srt,
                            [[frag_coord]] const float4 frag_co,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  float4 color = texture(srt.image, v_out.uv);
  if (srt.img_premultiplied) {
    color_alpha_unpremultiply(color, color);
  }

  frag_out.color = float4(0.0);
  int phase = int(mod((frag_co.x + frag_co.y), 6.0f));
  if (any(greaterThan(color.rgb, float3(srt.zebra_limit)))) {
    if (phase == 4) {
      frag_out.color = float4(0.0f, 0.0f, 0.0f, 0.85f);
    }
    else if (phase >= 3) {
      frag_out.color = float4(1.0f, 0.0f, 0.5f, 0.95f);
    }
  }
}
}  // namespace sequencer::zebra

PipelineGraphic gpu_shader_sequencer_zebra(sequencer::zebra::main_vert,
                                           sequencer::zebra::main_frag);
