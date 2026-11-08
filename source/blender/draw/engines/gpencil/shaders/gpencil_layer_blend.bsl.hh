/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpencil_blend_mode.bsl.hh"
#include "gpu_shader_fullscreen.bsl.hh"

namespace gpencil::layer_blend {

[[vertex]] void vert_main([[vertex_id]] const int vert_id, [[position]] float4 &out_pos)
{
  fullscreen_vertex(vert_id, out_pos);
}

struct FragOut {
  /* Reminder: This is considered SRC color in blend equations.
   * Same operation on all buffers. */
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float4 revealage;
};

struct Resources {
  [[sampler(0)]] sampler2D color_buf;
  [[sampler(1)]] sampler2D reveal_buf;
  [[sampler(2)]] sampler2D mask_buf;
  [[push_constant]] int blend_mode;
  [[push_constant]] float blend_opacity;
};

[[fragment]] void layer_blend_frag([[resource_table]] const Resources &srt,
                                   [[frag_coord]] const float4 frag_co,
                                   [[out]] FragOut &out_frag)
{
  float2 screen_uv = frag_co.xy / float2(textureSize(srt.color_buf, 0).xy);

  float4 color;

  /* Remember, this is associated alpha (aka. pre-multiply). */
  color.rgb = textureLod(srt.color_buf, screen_uv, 0).rgb;
  /* Stroke only render mono-chromatic revealage. We convert to alpha. */
  color.a = 1.0f - textureLod(srt.reveal_buf, screen_uv, 0).r;

  float mask = textureLod(srt.mask_buf, screen_uv, 0).r;
  mask *= srt.blend_opacity;

  out_frag.color = float4(1.0f, 0.0f, 1.0f, 1.0f);
  out_frag.revealage = float4(1.0f, 0.0f, 1.0f, 1.0f);

  blend_mode_output(srt.blend_mode, color, mask, out_frag.color, out_frag.revealage);
}

[[fragment]]
void mask_invert_frag([[out]] FragOut &out_frag)
{
  /* Blend mode does the inversion. */
  out_frag.revealage = out_frag.color = float4(1.0f);
}

}  // namespace gpencil::layer_blend

PipelineGraphic gpencil_layer_blend(gpencil::layer_blend::vert_main,
                                    gpencil::layer_blend::layer_blend_frag);
PipelineGraphic gpencil_mask_invert(gpencil::layer_blend::vert_main,
                                    gpencil::layer_blend::mask_invert_frag);
