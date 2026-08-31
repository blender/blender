/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "GPU_shader_shared.hh"
#include "gpu_shader_utildefines_lib.glsl"

#define GPENCIL_FLATCAP 1

namespace builtin::annotation {

/* Get Z-depth value. */
float get_Z_depth(float4 point)
{
  return (point.z / point.w);
}

/* check equality but with a small tolerance */
bool is_equal(float4 p1, float4 p2)
{
  float limit = 0.0001f;
  float x = abs(p1.x - p2.x);
  float y = abs(p1.y - p2.y);
  float z = abs(p1.z - p2.z);

  if ((x < limit) && (y < limit) && (z < limit)) {
    return true;
  }

  return false;
}

struct VertOut {
  float4 gpu_position;
  float4 final_color;
  float final_thickness;
};

struct GeomOut {
  float4 gpu_position;
  float2 tex_coord;
  float4 final_color;
};

struct Resources {
  [[storage(0, read), frequency(GEOMETRY)]] const GreasePencilStrokeData (&gp_vert_data)[];
  [[uniform(0)]] const GPencilStrokeData gpencil_stroke_data;

  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[push_constant]] float4x4 ProjectionMatrix;

  /* project 3d point to 2d on screen space */
  float2 to_screen_space(float4 vert) const
  {
    return float2(vert.xy / vert.w) * gpencil_stroke_data.viewport;
  }

  GreasePencilStrokeData input_assembly(uint in_vertex_id) const
  {
    /* Assume no index buffer. */
    return gp_vert_data[in_vertex_id];
  }

  VertOut vertex_main(GreasePencilStrokeData vert_in) const
  {
    float thickness_scale = 1 / gpencil_stroke_data.pixsize;

    VertOut vert_out;
    vert_out.gpu_position = ModelViewProjectionMatrix * float4(vert_in.position, 1.0f);
    vert_out.final_color = vert_in.stroke_color;

    float size = (ProjectionMatrix[3][3] == 0.0f) ?
                     (vert_in.stroke_thickness * thickness_scale / vert_out.gpu_position.z) :
                     (vert_in.stroke_thickness * thickness_scale);
    vert_out.final_thickness = max(size * gpencil_stroke_data.objscale, 1.0f);
    return vert_out;
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

  void geometry_main(GeomOut &selected_vert,
                     VertOut geom_in[4],
                     uint out_vertex_id,
                     uint out_primitive_id) const
  {
    constexpr float MiterLimit = 0.75f;

    float4 P0 = geom_in[0].gpu_position;
    float4 P1 = geom_in[1].gpu_position;
    float4 P2 = geom_in[2].gpu_position;
    float4 P3 = geom_in[3].gpu_position;

    /* Get the four vertices passed to the shader. */
    float2 sp0 = to_screen_space(P0); /* start of previous segment */
    float2 sp1 = to_screen_space(P1); /* end of previous segment, start of current segment */
    float2 sp2 = to_screen_space(P2); /* end of current segment, start of next segment */
    float2 sp3 = to_screen_space(P3); /* end of next segment */

    /* Culling outside viewport. */
    float2 area = gpencil_stroke_data.viewport * 4.0f;
    if (sp1.x < -area.x || sp1.x > area.x) {
      return;
    }
    if (sp1.y < -area.y || sp1.y > area.y) {
      return;
    }
    if (sp2.x < -area.x || sp2.x > area.x) {
      return;
    }
    if (sp2.y < -area.y || sp2.y > area.y) {
      return;
    }

    /* determine the direction of each of the 3 segments (previous, current, next) */
    float2 v0 = normalize(sp1 - sp0);
    float2 v1 = normalize(sp2 - sp1);
    float2 v2 = normalize(sp3 - sp2);

    /* determine the normal of each of the 3 segments (previous, current, next) */
    float2 n0 = float2(-v0.y, v0.x);
    float2 n1 = float2(-v1.y, v1.x);
    float2 n2 = float2(-v2.y, v2.x);

    /* determine miter lines by averaging the normals of the 2 segments */
    float2 miter_a = normalize(n0 + n1); /* miter at start of current segment */
    float2 miter_b = normalize(n1 + n2); /* miter at end of current segment */

    /* determine the length of the miter by projecting it onto normal and then inverse it */
    float an1 = dot(miter_a, n1);
    float bn1 = dot(miter_b, n2);
    if (an1 == 0.0f) {
      an1 = 1.0f;
    }
    if (bn1 == 0.0f) {
      bn1 = 1.0f;
    }

    float length_a = geom_in[1].final_thickness / an1;
    float length_b = geom_in[2].final_thickness / bn1;
    if (length_a <= 0.0f) {
      length_a = 0.01f;
    }
    if (length_b <= 0.0f) {
      length_b = 0.01f;
    }

    GeomOut geom_out;

    /* prevent excessively long miters at sharp corners */
    if (dot(v0, v1) < -MiterLimit) {
      miter_a = n1;
      length_a = geom_in[1].final_thickness;

      /* close the gap */
      if (dot(v0, n1) > 0.0f) {
        geom_out.tex_coord = float2(0.0f, 0.0f);
        geom_out.final_color = geom_in[1].final_color;
        geom_out.gpu_position = float4((sp1 + geom_in[1].final_thickness * n0) /
                                           gpencil_stroke_data.viewport,
                                       get_Z_depth(P1),
                                       1.0f);
        strip_emit_vertex(selected_vert, 0, out_vertex_id, out_primitive_id, geom_out);

        geom_out.tex_coord = float2(0.0f, 0.0f);
        geom_out.final_color = geom_in[1].final_color;
        geom_out.gpu_position = float4((sp1 + geom_in[1].final_thickness * n1) /
                                           gpencil_stroke_data.viewport,
                                       get_Z_depth(P1),
                                       1.0f);
        strip_emit_vertex(selected_vert, 1, out_vertex_id, out_primitive_id, geom_out);

        geom_out.tex_coord = float2(0.0f, 0.5f);
        geom_out.final_color = geom_in[1].final_color;
        geom_out.gpu_position = float4(sp1 / gpencil_stroke_data.viewport, get_Z_depth(P1), 1.0f);
        strip_emit_vertex(selected_vert, 2, out_vertex_id, out_primitive_id, geom_out);
      }
      else {
        geom_out.tex_coord = float2(0.0f, 1.0f);
        geom_out.final_color = geom_in[1].final_color;
        geom_out.gpu_position = float4((sp1 - geom_in[1].final_thickness * n1) /
                                           gpencil_stroke_data.viewport,
                                       get_Z_depth(P1),
                                       1.0f);
        strip_emit_vertex(selected_vert, 0, out_vertex_id, out_primitive_id, geom_out);

        geom_out.tex_coord = float2(0.0f, 1.0f);
        geom_out.final_color = geom_in[1].final_color;
        geom_out.gpu_position = float4((sp1 - geom_in[1].final_thickness * n0) /
                                           gpencil_stroke_data.viewport,
                                       get_Z_depth(P1),
                                       1.0f);
        strip_emit_vertex(selected_vert, 1, out_vertex_id, out_primitive_id, geom_out);

        geom_out.tex_coord = float2(0.0f, 0.5f);
        geom_out.final_color = geom_in[1].final_color;
        geom_out.gpu_position = float4(sp1 / gpencil_stroke_data.viewport, get_Z_depth(P1), 1.0f);
        strip_emit_vertex(selected_vert, 2, out_vertex_id, out_primitive_id, geom_out);
      }

      /* Restart the strip. */
      geom_out.gpu_position = float4(NAN_FLT);
      strip_emit_vertex(selected_vert, 3, out_vertex_id, out_primitive_id, geom_out);
    }

    if (dot(v1, v2) < -MiterLimit) {
      miter_b = n1;
      length_b = geom_in[2].final_thickness;
    }

    /* Generate the start end-cap (alpha < 0 used as end-cap flag). */
    float extend = gpencil_stroke_data.fill_stroke ? 2.0f : 1.0f;
    if ((gpencil_stroke_data.caps_start != GPENCIL_FLATCAP) && is_equal(P0, P2)) {
      geom_out.tex_coord = float2(1.0f, 0.5f);
      geom_out.final_color = float4(geom_in[1].final_color.rgb, geom_in[1].final_color.a * -1.0f);
      float2 svn1 = normalize(sp1 - sp2) * length_a * 4.0f * extend;
      geom_out.gpu_position = float4(
          (sp1 + svn1) / gpencil_stroke_data.viewport, get_Z_depth(P1), 1.0f);
      strip_emit_vertex(selected_vert, 4, out_vertex_id, out_primitive_id, geom_out);

      geom_out.tex_coord = float2(0.0f, 0.0f);
      geom_out.final_color = float4(geom_in[1].final_color.rgb, geom_in[1].final_color.a * -1.0f);
      geom_out.gpu_position = float4((sp1 - (length_a * 2.0f) * miter_a) /
                                         gpencil_stroke_data.viewport,
                                     get_Z_depth(P1),
                                     1.0f);
      strip_emit_vertex(selected_vert, 5, out_vertex_id, out_primitive_id, geom_out);

      geom_out.tex_coord = float2(0.0f, 1.0f);
      geom_out.final_color = float4(geom_in[1].final_color.rgb, geom_in[1].final_color.a * -1.0f);
      geom_out.gpu_position = float4((sp1 + (length_a * 2.0f) * miter_a) /
                                         gpencil_stroke_data.viewport,
                                     get_Z_depth(P1),
                                     1.0f);
      strip_emit_vertex(selected_vert, 6, out_vertex_id, out_primitive_id, geom_out);
    }

    /* generate the triangle strip */
    geom_out.tex_coord = float2(0.0f, 0.0f);
    geom_out.final_color = geom_in[1].final_color;
    geom_out.gpu_position = float4(
        (sp1 + length_a * miter_a) / gpencil_stroke_data.viewport, get_Z_depth(P1), 1.0f);
    strip_emit_vertex(selected_vert, 7, out_vertex_id, out_primitive_id, geom_out);

    geom_out.tex_coord = float2(0.0f, 1.0f);
    geom_out.final_color = geom_in[1].final_color;
    geom_out.gpu_position = float4(
        (sp1 - length_a * miter_a) / gpencil_stroke_data.viewport, get_Z_depth(P1), 1.0f);
    strip_emit_vertex(selected_vert, 8, out_vertex_id, out_primitive_id, geom_out);

    geom_out.tex_coord = float2(0.0f, 0.0f);
    geom_out.final_color = geom_in[2].final_color;
    geom_out.gpu_position = float4(
        (sp2 + length_b * miter_b) / gpencil_stroke_data.viewport, get_Z_depth(P2), 1.0f);
    strip_emit_vertex(selected_vert, 9, out_vertex_id, out_primitive_id, geom_out);

    geom_out.tex_coord = float2(0.0f, 1.0f);
    geom_out.final_color = geom_in[2].final_color;
    geom_out.gpu_position = float4(
        (sp2 - length_b * miter_b) / gpencil_stroke_data.viewport, get_Z_depth(P2), 1.0f);
    strip_emit_vertex(selected_vert, 10, out_vertex_id, out_primitive_id, geom_out);

    /* Generate the end end-cap (alpha < 0 used as end-cap flag). */
    if ((gpencil_stroke_data.caps_end != GPENCIL_FLATCAP) && is_equal(P1, P3)) {
      geom_out.tex_coord = float2(0.0f, 1.0f);
      geom_out.final_color = float4(geom_in[2].final_color.rgb, geom_in[2].final_color.a * -1.0f);
      geom_out.gpu_position = float4((sp2 + (length_b * 2.0f) * miter_b) /
                                         gpencil_stroke_data.viewport,
                                     get_Z_depth(P2),
                                     1.0f);
      strip_emit_vertex(selected_vert, 11, out_vertex_id, out_primitive_id, geom_out);

      geom_out.tex_coord = float2(0.0f, 0.0f);
      geom_out.final_color = float4(geom_in[2].final_color.rgb, geom_in[2].final_color.a * -1.0f);
      geom_out.gpu_position = float4((sp2 - (length_b * 2.0f) * miter_b) /
                                         gpencil_stroke_data.viewport,
                                     get_Z_depth(P2),
                                     1.0f);
      strip_emit_vertex(selected_vert, 12, out_vertex_id, out_primitive_id, geom_out);

      geom_out.tex_coord = float2(1.0f, 0.5f);
      geom_out.final_color = float4(geom_in[2].final_color.rgb, geom_in[2].final_color.a * -1.0f);
      float2 svn2 = normalize(sp2 - sp1) * length_b * 4.0f * extend;
      geom_out.gpu_position = float4(
          (sp2 + svn2) / gpencil_stroke_data.viewport, get_Z_depth(P2), 1.0f);
      strip_emit_vertex(selected_vert, 13, out_vertex_id, out_primitive_id, geom_out);
    }
  }
};

struct VertInterp {
  [[smooth]] float4 mColor;
  [[smooth]] float2 mTexCoord;
};

[[vertex]] void vert_main([[resource_table]] const Resources &srt,
                          [[vertex_id]] const int vert_id,
                          [[position]] float4 &out_pos,
                          [[out]] VertInterp &v_out)
{
  /* Line Strip Adjacency primitive. */
  constexpr uint input_primitive_vertex_count =
      1u; /* We read 4 but advance 1. Assume no restart. */
  /* Triangle list primitive (emulating triangle strip). */
  constexpr uint output_primitive_vertex_count = 3u;
  constexpr uint output_primitive_count = 12u;
  constexpr uint output_invocation_count = 1u;
  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(vert_id) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(vert_id) % output_primitive_vertex_count;
  uint out_primitive_id = (uint(vert_id) / output_primitive_vertex_count) % output_primitive_count;

  GreasePencilStrokeData vert_in[4];
  vert_in[0] = srt.input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = srt.input_assembly(in_primitive_first_vertex + 1u);
  vert_in[2] = srt.input_assembly(in_primitive_first_vertex + 2u);
  vert_in[3] = srt.input_assembly(in_primitive_first_vertex + 3u);

  VertOut vert_out[4];
  vert_out[0] = srt.vertex_main(vert_in[0]);
  vert_out[1] = srt.vertex_main(vert_in[1]);
  vert_out[2] = srt.vertex_main(vert_in[2]);
  vert_out[3] = srt.vertex_main(vert_in[3]);

  GeomOut out_vert;
  /* Discard by default. */
  out_vert.gpu_position = float4(NAN_FLT);
  srt.geometry_main(out_vert, vert_out, out_vertex_id, out_primitive_id);

  out_pos = out_vert.gpu_position;
  v_out.mTexCoord = out_vert.tex_coord;
  v_out.mColor = out_vert.final_color;
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void frag_main([[in]] const VertInterp &v_out, [[out]] FragOut &frag_out)
{
  constexpr float2 center = float2(0.0f, 0.5f);
  float4 tColor = v_out.mColor;
  /* if alpha < 0, then encap */
  if (tColor.a < 0.0f) {
    tColor.a = tColor.a * -1.0f;
    float dist = length(v_out.mTexCoord - center);
    if (dist > 0.25f) {
      gpu_discard_fragment();
    }
  }
  /* Solid */
  frag_out.color = tColor;
}

}  // namespace builtin::annotation

PipelineGraphic gpu_shader_annotation(builtin::annotation::vert_main,
                                      builtin::annotation::frag_main);
