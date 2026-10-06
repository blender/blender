/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "overlay_line_packing.bsl.hh"
#include "overlay_shader_shared.hh"
#include "select_lib.bsl.hh"

/* Wire Color Types, matching eV3DShadingColorType. */
enum eV3DShadingColorType : int {
  V3D_SHADING_SINGLE_COLOR = 2,
  V3D_SHADING_OBJECT_COLOR = 4,
  V3D_SHADING_RANDOM_COLOR = 1,
};

namespace overlay {

struct ClippingConstant {
  [[compilation_constant]] bool use_clipping;
};

struct Clipping {
  [[resource_table]] ClippingConstant constants;

  [[uniform(DRW_CLIPPING_UBO_SLOT), condition(use_clipping)]] const float4 (&drw_clipping_)[6];

  void set_clipping_distances(float3 ws_P,
                              float &dist0,
                              float &dist1,
                              float &dist2,
                              float &dist3,
                              float &dist4,
                              float &dist5) const
  {
    if (constants.use_clipping) [[static_branch]] {
      float4 pos_4d = float4(ws_P, 1.0f);
      dist0 = dot(drw_clipping_[0], pos_4d);
      dist1 = dot(drw_clipping_[1], pos_4d);
      dist2 = dot(drw_clipping_[2], pos_4d);
      dist3 = dot(drw_clipping_[3], pos_4d);
      dist4 = dot(drw_clipping_[4], pos_4d);
      dist5 = dot(drw_clipping_[5], pos_4d);
    }
  }
};

struct VertOut {
  [[flat]] float4 final_color;
  [[flat]] float2 edge_start;
  [[no_perspective]] float2 edge_pos;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float4 line_output;
};

struct Uniform {
  [[uniform(OVERLAY_GLOBALS_SLOT)]] UniformData uniform_buf;

  /* project to screen space */
  float2 proj(float4 pos) const
  {
    return (0.5f * (pos.xy / pos.w) + 0.5f) * uniform_buf.size_viewport;
  }
};

float2 compute_dir(float2 v0, float2 /*v1*/, float2 v2)
{
  float2 dir = normalize(v2 - v0);
  dir = float2(dir.y, -dir.x);
  return dir;
}

float4x4 extract_matrix_packed_data(float4x4 mat, float4 &dataA, float4 &dataB)
{
  constexpr float div = 1.0f / 255.0f;
  int a = int(mat[0][3]);
  int b = int(mat[1][3]);
  int c = int(mat[2][3]);
  int d = int(mat[3][3]);
  dataA = float4(a & 0xFF, a >> 8, b & 0xFF, b >> 8) * div;
  dataB = float4(c & 0xFF, c >> 8, d & 0xFF, d >> 8) * div;
  mat[0][3] = mat[1][3] = mat[2][3] = 0.0f;
  mat[3][3] = 1.0f;
  return mat;
}

[[fragment]]
void wire_frag([[resource_table, condition(selectable)]] draw::Select &sel,
               [[frag_coord]] const float4 frag_co,
               [[in]] const VertOut &v_out,
               [[in, condition(selectable)]] const draw::SelectOut &sel_out,
               [[out]] FragOut &frag_out)
{
  frag_out.line_output = overlay::pack_line_data(frag_co.xy, v_out.edge_start, v_out.edge_pos);
  frag_out.color = v_out.final_color;
  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

}  // namespace overlay
