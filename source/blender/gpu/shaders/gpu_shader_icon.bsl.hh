/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "GPU_shader_shared.hh"

namespace builtin::icon {

struct Resource {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[push_constant]] float4 finalColor;
  [[push_constant]] float4 rect_icon;
  [[push_constant]] float4 rect_geom;
  [[push_constant]] float text_width;

  [[sampler(0)]] sampler2D image;
  [[uniform(0)]] MultiIconCallData &multi_icon_data;
};

struct VertOut {
  [[flat]] float4 color;
  [[smooth]] float2 uv;
  [[smooth]] float2 mask_coord;
};

struct VertIn {
  [[attribute(0)]] float2 pos;
};

/**
 * Simple shader that just draw multiple icons at the specified locations
 * does not need any vertex input (producing less call to immBegin/End)
 */
[[vertex]] void main_vert([[resource_table]] const Resource &srt,
                          [[instance_index]] const int inst_id,
                          [[position]] float4 &out_pos,
                          [[in]] const VertIn &v_in,
                          [[out]] VertOut &v_out)
{
  float4 rect = srt.multi_icon_data.calls_data[inst_id * 3];
  float4 tex = srt.multi_icon_data.calls_data[inst_id * 3 + 1];
  v_out.color = srt.multi_icon_data.calls_data[inst_id * 3 + 2];

  /* Use pos to select the right swizzle (instead of gl_VertexID)
   * in order to workaround an OSX driver bug. */
  if (all(equal(v_in.pos, float2(0.0f, 0.0f)))) {
    rect.xy = rect.xz;
    tex.xy = tex.xz;
  }
  else if (all(equal(v_in.pos, float2(0.0f, 1.0f)))) {
    rect.xy = rect.xw;
    tex.xy = tex.xw;
  }
  else if (all(equal(v_in.pos, float2(1.0f, 1.0f)))) {
    rect.xy = rect.yw;
    tex.xy = tex.yw;
  }
  else {
    rect.xy = rect.yz;
    tex.xy = tex.yz;
  }

  out_pos = float4(rect.xy, 0.0f, 1.0f);
  v_out.uv = tex.xy;
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

/**
 * Draw the icons, leaving a semi-transparent rectangle on top of the icon.
 *
 * The top-left corner of the rectangle is rounded and drawn with anti-alias.
 * The anti-alias is done by transitioning from the outer to the inner radius of
 * the rounded corner, and the rectangle sides.
 */
[[fragment]] void main_frag([[resource_table]] const Resource &srt,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  /* Sample texture with LOD BIAS. Used instead of custom LOD bias in GPU_SAMPLER_CUSTOM_ICON. */
  frag_out.color = texture(srt.image, v_out.uv, -0.5f) * v_out.color;
}

}  // namespace builtin::icon

PipelineGraphic gpu_shader_icon(builtin::icon::main_vert, builtin::icon::main_frag);
