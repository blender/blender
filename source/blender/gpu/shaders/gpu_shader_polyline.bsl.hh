/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_colorspace.bsl.hh"
#include "gpu_shader_index_load.bsl.hh"
#include "gpu_shader_utildefines_lib.glsl"

namespace builtin::polyline {

static constexpr float SMOOTH_WIDTH = 1.0f;

/* Using vertex pulling from a storage buffer. */
struct VertIn {
  float3 ls_P;
  float4 final_color;
};

struct VertOut {
  float4 gpu_position;
  float4 final_color;
  float clip;
};

struct GeomOut {
  float4 gpu_position;
  float4 final_color;
  float clip;
  float smoothline;
};

struct UniformColor {
  [[push_constant]] const float4 color;
};

struct Resources {
  [[compilation_constant]] const bool use_clipping;
  [[compilation_constant]] const bool use_color_uniform;
  [[compilation_constant]] const bool use_color_flat;
  [[compilation_constant]] const bool use_color_smooth;

  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[push_constant]] float2 viewportSize;
  [[push_constant]] float lineWidth;
  [[push_constant]] bool lineSmooth;

  [[push_constant]] float4x4 ModelMatrix;
  [[push_constant]] float4 ClipPlane;

  [[push_constant]] const int3 gpu_vert_stride_count_offset;

  [[storage(GPU_SSBO_POLYLINE_POS_BUF_SLOT, read), frequency(GEOMETRY)]] const float (&pos)[];
  [[push_constant]] const int2 gpu_attr_0;
  /* Number of component per vertex. */
  [[push_constant]] const int gpu_attr_0_len;
  /* True if attribute is in int format and we need to fetch as int (not normalized). */
  [[push_constant]] const bool gpu_attr_0_fetch_int;

  [[storage(GPU_SSBO_POLYLINE_COL_BUF_SLOT, read),
    frequency(GEOMETRY)]] [[condition(use_color_uniform == 0)]] const float (&color)[];
  [[push_constant]] const int2 gpu_attr_1;
  [[push_constant]] const int gpu_attr_1_len;
  [[push_constant]] const bool gpu_attr_1_fetch_unorm8;

  [[resource_table]] [[condition(use_color_uniform == 1)]] srt_t<UniformColor> uniform_col;

  [[resource_table]] srt_t<IndexLoad> index_load;

  VertIn pull_vertex_data(uint in_vertex_id) const
  {
    [[resource_table]] const IndexLoad &index = index_load;
    uint v_i = index.load(in_vertex_id);
    uint ofs = uint(gpu_vert_stride_count_offset.z);

    VertIn vert_in;
    vert_in.ls_P = float3(0.0f, 0.0f, 0.0f);
    /* Need to support 1, 2 and 3 dimensional input (sigh). */
    vert_in.ls_P.x = pos[gpu_attr_load_index(v_i, gpu_attr_0) + 0 + ofs];
    if (gpu_attr_0_len >= 2) {
      vert_in.ls_P.y = pos[gpu_attr_load_index(v_i, gpu_attr_0) + 1 + ofs];
    }
    if (gpu_attr_0_len >= 3) {
      vert_in.ls_P.z = pos[gpu_attr_load_index(v_i, gpu_attr_0) + 2 + ofs];
    }

    if (gpu_attr_0_fetch_int) {
      vert_in.ls_P = float3(floatBitsToInt(vert_in.ls_P));
    }

    vert_in.final_color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    if (!use_color_uniform) [[static_branch]] {
      /* Need to support 1, 2, 3 and 4 dimensional input (sigh). */
      vert_in.final_color.x = color[gpu_attr_load_index(v_i, gpu_attr_1) + 0 + ofs];
      if (gpu_attr_1_fetch_unorm8) {
        vert_in.final_color = unpackUnorm4x8(floatBitsToUint(vert_in.final_color.x));
      }
      else {
        if (gpu_attr_1_len >= 2) {
          vert_in.final_color.y = color[gpu_attr_load_index(v_i, gpu_attr_1) + 1 + ofs];
        }
        if (gpu_attr_1_len >= 3) {
          vert_in.final_color.z = color[gpu_attr_load_index(v_i, gpu_attr_1) + 2 + ofs];
        }
        if (gpu_attr_1_len >= 4) {
          vert_in.final_color.w = color[gpu_attr_load_index(v_i, gpu_attr_1) + 3 + ofs];
        }
      }
    }
    return vert_in;
  }

  VertOut vertex_main(VertIn vert_in) const
  {
    VertOut vert_out;
    vert_out.gpu_position = ModelViewProjectionMatrix * float4(vert_in.ls_P, 1.0f);
    vert_out.final_color = vert_in.final_color;
    vert_out.clip = dot(ModelMatrix * float4(vert_in.ls_P, 1.0f), ClipPlane);
    return vert_out;
  }

  /* Clips point to near clip plane before perspective divide. */
  float4 clip_line_point_homogeneous_space(float4 p, float4 q) const
  {
    if (p.z < -p.w) {
      /* Just solves p + (q - p) * A; for A when p.z / p.w = -1.0f. */
      float denom = q.z - p.z + q.w - p.w;
      if (denom == 0.0f) {
        /* No solution. */
        return p;
      }
      float A = (-p.z - p.w) / denom;
      p = p + (q - p) * A;
    }
    return p;
  }

  void strip_emit_vertex(GeomOut &selected_vert,
                         const uint strip_index,
                         uint out_vertex_id,
                         uint out_primitive_id,
                         GeomOut geom_out) const
  {
    bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
    /* Maps triangle list primitives to triangle strip indices. */
    uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                           out_primitive_id;

    if (out_strip_index == strip_index) {
      selected_vert = geom_out;
    }
  }

  /* Runs once per line vertex. Emit 2 vertices. */
  void do_line_vertex(GeomOut &selected_vert,
                      const uint i,
                      uint out_vertex_id,
                      uint out_primitive_id,
                      VertOut geom_in[2],
                      float4 position,
                      float2 ofs) const
  {
    GeomOut geom_out;
    if (use_color_uniform) [[static_branch]] {
      [[resource_table]] const UniformColor &uni = uniform_col;
      geom_out.final_color = uni.color;
    }
    if (use_color_flat) [[static_branch]] {
      /* WATCH: Assuming last provoking vertex. */
      geom_out.final_color = geom_in[1].final_color;
    }
    if (use_color_smooth) [[static_branch]] {
      geom_out.final_color = geom_in[i].final_color;
    }

    geom_out.clip = geom_in[i].clip;

    geom_out.smoothline = (lineWidth + SMOOTH_WIDTH * float(lineSmooth)) * 0.5f;
    geom_out.gpu_position = position;
    geom_out.gpu_position.xy += ofs * position.w;
    strip_emit_vertex(selected_vert, i * 2u + 0u, out_vertex_id, out_primitive_id, geom_out);

    geom_out.smoothline = -(lineWidth + SMOOTH_WIDTH * float(lineSmooth)) * 0.5f;
    geom_out.gpu_position = position;
    geom_out.gpu_position.xy -= ofs * position.w;
    strip_emit_vertex(selected_vert, i * 2u + 1u, out_vertex_id, out_primitive_id, geom_out);
  }

  void geometry_main(GeomOut &selected_vert,
                     VertOut geom_in[2],
                     uint out_vertex_id,
                     uint out_primitive_id,
                     uint /*out_invocation_id*/) const
  {
    float4 p0 = clip_line_point_homogeneous_space(geom_in[0].gpu_position,
                                                  geom_in[1].gpu_position);
    float4 p1 = clip_line_point_homogeneous_space(geom_in[1].gpu_position,
                                                  geom_in[0].gpu_position);
    float2 e = normalize(((p1.xy / p1.w) - (p0.xy / p0.w)) * viewportSize.xy);

#if 0 /* Hard turn when line direction changes quadrant. */
    e = abs(e);
    float2 ofs = (e.x > e.y) ? float2(0.0f, 1.0f / e.x) : float2(1.0f / e.y, 0.0f);
#else /* Use perpendicular direction. */
    float2 ofs = float2(-e.y, e.x);
#endif
    ofs /= viewportSize.xy;
    ofs *= lineWidth + SMOOTH_WIDTH * float(lineSmooth);

    do_line_vertex(selected_vert, 0u, out_vertex_id, out_primitive_id, geom_in, p0, ofs);
    do_line_vertex(selected_vert, 1u, out_vertex_id, out_primitive_id, geom_in, p1, ofs);
  }
};

struct VertInterp {
  [[smooth]] float4 final_color;
  [[smooth]] float clip;
  [[no_perspective]] float smoothline;
};

[[vertex]] void vert_main([[resource_table]] const Resources &srt,
                          [[vertex_id]] const int vert_id,
                          [[position]] float4 &out_pos,
                          [[out]] VertInterp &v_out)
{
  /* Line list primitive. */
  uint input_primitive_vertex_count = uint(srt.gpu_vert_stride_count_offset.x);
  /* Triangle list primitive (emulating triangle strip). */
  constexpr uint output_primitive_vertex_count = 3u;
  constexpr uint output_primitive_count = 2u;
  constexpr uint output_invocation_count = 1u;
  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(vert_id) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(vert_id) % output_primitive_vertex_count;
  uint out_primitive_id = (uint(vert_id) / output_primitive_vertex_count) % output_primitive_count;
  uint out_invocation_id = (uint(vert_id) / output_vertex_count_per_invocation) %
                           output_invocation_count;
  /* Used to wrap around for the line loop case. */
  uint input_total_vertex_count = uint(srt.gpu_vert_stride_count_offset.y);

  VertIn vert_in[2];
  vert_in[0] = srt.pull_vertex_data(in_primitive_first_vertex + 0u);
  vert_in[1] = srt.pull_vertex_data((in_primitive_first_vertex + 1u) % input_total_vertex_count);

  VertOut vert_out[2];
  vert_out[0] = srt.vertex_main(vert_in[0]);
  vert_out[1] = srt.vertex_main(vert_in[1]);

  GeomOut out_vert;
  /* Discard by default. */
  out_vert.gpu_position = float4(NAN_FLT);
  srt.geometry_main(out_vert, vert_out, out_vertex_id, out_primitive_id, out_invocation_id);

  out_pos = out_vert.gpu_position;
  v_out.final_color = out_vert.final_color;
  v_out.smoothline = out_vert.smoothline;
  v_out.clip = out_vert.clip;
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void frag_main([[resource_table]] const Resources &srt,
                            [[resource_table]] const ColorSpace &colorspace,
                            [[in]] const VertInterp &v_out,
                            [[out]] FragOut &frag_out)
{
  if (srt.use_clipping) [[static_branch]] {
    if (v_out.clip < 0.0f) {
      gpu_discard_fragment();
    }
  }
  frag_out.color = v_out.final_color;
  if (srt.lineSmooth) {
    frag_out.color.a *= clamp(
        (srt.lineWidth + SMOOTH_WIDTH) * 0.5f - abs(v_out.smoothline), 0.0f, 1.0f);
  }
  frag_out.color = colorspace.rec709_srgb_to_output_space(frag_out.color);
}

}  // namespace builtin::polyline

#ifndef GLSL_CPP_STUBS
PipelineGraphic gpu_shader_polyline_flat_color(builtin::polyline::vert_main,
                                               builtin::polyline::frag_main,
                                               builtin::polyline::Resources{
                                                   .use_clipping = false,
                                                   .use_color_uniform = false,
                                                   .use_color_flat = true,
                                                   .use_color_smooth = false,
                                               });
PipelineGraphic gpu_shader_polyline_smooth_color(builtin::polyline::vert_main,
                                                 builtin::polyline::frag_main,
                                                 builtin::polyline::InputAssembly{
                                                     .use_clipping = false,
                                                     .use_color_uniform = false,
                                                     .use_color_flat = false,
                                                     .use_color_smooth = true,
                                                 });
PipelineGraphic gpu_shader_polyline_uniform_color(builtin::polyline::vert_main,
                                                  builtin::polyline::frag_main,
                                                  builtin::polyline::InputAssembly{
                                                      .use_clipping = false,
                                                      .use_color_uniform = true,
                                                      .use_color_flat = false,
                                                      .use_color_smooth = false,
                                                  });
PipelineGraphic gpu_shader_polyline_uniform_color_clipped(builtin::polyline::vert_main,
                                                          builtin::polyline::frag_main,
                                                          builtin::polyline::InputAssembly{
                                                              .use_clipping = true,
                                                              .use_color_uniform = true,
                                                              .use_color_flat = false,
                                                              .use_color_smooth = false,
                                                          });
#endif
