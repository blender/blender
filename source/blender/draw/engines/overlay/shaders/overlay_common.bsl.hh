/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_shader_shared.hh"
#include "gpu_shader_utildefines.bsl.hh"
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

float3 hsv_to_rgb(float3 hsv)
{
  float3 nrgb = abs(hsv.x * 6.0f - float3(3.0f, 2.0f, 4.0f)) * float3(1, -1, -1) +
                float3(-1, 2, 2);
  nrgb = saturate(nrgb);
  return ((nrgb - 1.0f) * hsv.y + 1.0f) * hsv.z;
}

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

struct VertOutSmooth {
  [[smooth]] float4 final_color;
  [[flat]] float2 edge_start;
  [[no_perspective]] float2 edge_pos;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float4 line_output;
};

struct WireColor {
  float3 rim, wire;
};

struct Uniform {
  [[uniform(OVERLAY_GLOBALS_SLOT)]] UniformData uniform_buf;

  /* project to screen space */
  float2 proj(float4 pos) const
  {
    return (0.5f * (pos.xy / pos.w) + 0.5f) * uniform_buf.size_viewport;
  }

  WireColor wire_color_get(eObjectInfoFlag ob_flag, bool use_coloring, bool is_transform) const
  {
    bool is_selected = flag_test(ob_flag, OBJECT_SELECTED);
    bool is_from_set = flag_test(ob_flag, OBJECT_FROM_SET);
    bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);
    const UniformData &theme = this->uniform_buf;

    WireColor col;
    if (is_from_set) {
      col.rim = theme.colors.wire.rgb;
      col.wire = theme.colors.wire.rgb;
    }
    else if (is_selected && use_coloring) {
      if (is_transform) {
        col.rim = theme.colors.transform.rgb;
      }
      else if (is_active) {
        col.rim = theme.colors.active_object.rgb;
      }
      else {
        col.rim = theme.colors.object_select.rgb;
      }
      col.wire = theme.colors.wire.rgb;
    }
    else {
      col.rim = theme.colors.wire.rgb;
      col.wire = theme.colors.background.rgb;
    }
    return col;
  }

  WireColor wire_object_color_get(ObjectInfos info,
                                  eV3DShadingColorType color_type,
                                  bool use_coloring) const
  {
    const bool is_selected = flag_test(info.flag, OBJECT_SELECTED);

    WireColor col;
    if (color_type == V3D_SHADING_OBJECT_COLOR) {
      col.rim = col.wire = info.ob_color.rgb * 0.5f;
    }
    else {
      float hue = info.random;
      float3 hsv = float3(hue, 0.75f, 0.8f);
      col.rim = col.wire = hsv_to_rgb(hsv);
    }

    if (is_selected && use_coloring) {
      /* "Normalize" color. */
      col.wire += 1e-4f; /* Avoid division by 0. */
      float brightness = max(col.wire.x, max(col.wire.y, col.wire.z));
      col.wire *= 0.5f / brightness;
      col.rim += 0.75f;
    }
    else {
      col.wire += 0.5f;
      col.rim *= 0.5f;
    }
    return col;
  }
};

struct WeightMap {
  [[resource_table]] Uniform uni;

  [[sampler(0)]] sampler1D weight_tx;

  float3 weight_to_rgb(float weight) const
  {
    const UniformData &theme = uni.uniform_buf;

    constexpr float no_active_weight = 666.0f;
    if (weight == no_active_weight) {
      /* No weight. */
      return theme.colors.wire.rgb;
    }
    if (weight > 1.0f || weight < 0.0f) {
      /* Error color */
      return float3(1.0f, 0.0f, 1.0f);
    }
    return texture(weight_tx, weight).rgb;
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

/* View-space Z is used to adjust for perspective projection.
 * Homogenous W is used to convert from NDC to homogenous space.
 * Offset is in view-space, so positive values are closer to the camera. */
float get_homogenous_z_offset(float4x4 winmat, float vs_z, float hs_w, float vs_offset)
{
  if (vs_offset == 0.0f) {
    /* Don't calculate homogenous offset if view-space offset is zero. */
    return 0.0f;
  }
  if (winmat[3][3] == 0.0f) {
    /* Clamp offset to half of Z to avoid floating point precision errors. */
    vs_offset = min(vs_offset, vs_z * -0.5f);
    /* From "Projection Matrix Tricks" by Eric Lengyel:
     * http://www.terathon.com/gdc07_lengyel.pdf (p. 24 Depth Modification) */
    return winmat[3][2] * (vs_offset / (vs_z * (vs_z + vs_offset))) * hs_w;
  }
  return winmat[2][2] * vs_offset * hs_w;
}

float3 non_linear_blend_color(float3 col1, float3 col2, float fac)
{
  col1 = pow(col1, float3(1.0f / 2.2f));
  col2 = pow(col2, float3(1.0f / 2.2f));
  float3 col = mix(col1, col2, fac);
  return pow(col, float3(2.2f));
}

float mul_project_m4_v3_zfac(ViewMatrices view, float pixel_fac, float3 co)
{
  float3 vP = view.point_world_to_view(co).xyz;
  float4x4 winmat = view.winmat;
  return pixel_fac *
         (winmat[0][3] * vP.x + winmat[1][3] * vP.y + winmat[2][3] * vP.z + winmat[3][3]);
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

[[fragment]]
void wire_frag_smooth([[resource_table, condition(selectable)]] draw::Select &sel,
                      [[frag_coord]] const float4 frag_co,
                      [[in]] const VertOutSmooth &v_out,
                      [[in, condition(selectable)]] const draw::SelectOut &sel_out,
                      [[out]] FragOut &frag_out)
{
  frag_out.line_output = overlay::pack_line_data(frag_co.xy, v_out.edge_start, v_out.edge_pos);
  frag_out.color = v_out.final_color;
  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

[[fragment]]
void point_frag([[resource_table, condition(selectable)]] draw::Select &sel,
                [[frag_coord]] const float4 frag_co,
                [[in]] const VertOut &v_out,
                [[out]] FragOut &frag_out,
                [[in, condition(selectable)]] const draw::SelectOut &sel_out,
                [[point_coord]] const float2 point_co)
{
  float2 centered = point_co - float2(0.5f);
  float dist_squared = dot(centered, centered);
  constexpr float rad_squared = 0.25f;
  /* Round point with jagged edges. */
  if (dist_squared > rad_squared) {
    if (!sel.consts.selectable) [[static_branch]] {
      gpu_discard_fragment();
    }
    return;
  }

  frag_out.color = v_out.final_color;
  frag_out.line_output = float4(0.0f);

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

}  // namespace overlay
