/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_gpencil_stroke_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_gpencil_stroke)

#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#define GP_XRAY_FRONT 0
#define GP_XRAY_3DSPACE 1
#define GP_XRAY_BACK 2

#define GPENCIL_FLATCAP 1

/* project 3d point to 2d on screen space */
float2 toScreenSpace(float4 vert)
{
  return float2(vert.xy / vert.w) * gpencil_stroke_data.viewport;
}

/* Get Z-depth value. */
float getZdepth(float4 point)
{
  if (gpencil_stroke_data.xraymode == GP_XRAY_FRONT) {
    return 0.0f;
  }
  if (gpencil_stroke_data.xraymode == GP_XRAY_3DSPACE) {
    return (point.z / point.w);
  }
  if (gpencil_stroke_data.xraymode == GP_XRAY_BACK) {
    return 1.0f;
  }

  /* in front by default */
  return 0.0f;
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

GreasePencilStrokeData input_assembly(uint in_vertex_id)
{
  /* Assume no index buffer. */
  return gp_vert_data[in_vertex_id];
}

struct VertOut {
  float4 gpu_position;
  float4 final_color;
  float final_thickness;
};

VertOut vertex_main(GreasePencilStrokeData vert_in)
{
  float defaultpixsize = gpencil_stroke_data.pixsize * (1000.0f / gpencil_stroke_data.pixfactor);

  VertOut vert_out;
  vert_out.gpu_position = ModelViewProjectionMatrix * float4(vert_in.position, 1.0f);
  vert_out.final_color = vert_in.stroke_color;

  if (gpencil_stroke_data.keep_size) {
    vert_out.final_thickness = vert_in.stroke_thickness;
  }
  else {
    float size = (ProjectionMatrix[3][3] == 0.0f) ?
                     (vert_in.stroke_thickness / (vert_out.gpu_position.z * defaultpixsize)) :
                     (vert_in.stroke_thickness / defaultpixsize);
    vert_out.final_thickness = max(size * gpencil_stroke_data.objscale, 1.0f);
  }
  return vert_out;
}

struct GeomOut {
  float4 gpu_position;
  float2 tex_coord;
  float4 final_color;
};

void export_vertex(GeomOut geom_out)
{
  gl_Position = geom_out.gpu_position;
  interp.mTexCoord = geom_out.tex_coord;
  interp.mColor = geom_out.final_color;
}

void strip_EmitVertex(const uint strip_index,
                      uint out_vertex_id,
                      uint out_primitive_id,
                      GeomOut geom_out)
{
  bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
  /* Maps triangle list primitives to triangle strip indices. */
  uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                         out_primitive_id;

  if (out_strip_index == strip_index) {
    export_vertex(geom_out);
  }
}

void geometry_main(VertOut geom_in[4], uint out_vertex_id, uint out_primitive_id)
{
  constexpr float MiterLimit = 0.75f;

  float4 P0 = geom_in[0].gpu_position;
  float4 P1 = geom_in[1].gpu_position;
  float4 P2 = geom_in[2].gpu_position;
  float4 P3 = geom_in[3].gpu_position;

  /* Get the four vertices passed to the shader. */
  float2 sp0 = toScreenSpace(P0); /* start of previous segment */
  float2 sp1 = toScreenSpace(P1); /* end of previous segment, start of current segment */
  float2 sp2 = toScreenSpace(P2); /* end of current segment, start of next segment */
  float2 sp3 = toScreenSpace(P3); /* end of next segment */

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
  if (an1 == 0) {
    an1 = 1;
  }
  if (bn1 == 0) {
    bn1 = 1;
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
    if (dot(v0, n1) > 0) {
      geom_out.tex_coord = float2(0, 0);
      geom_out.final_color = geom_in[1].final_color;
      geom_out.gpu_position = float4((sp1 + geom_in[1].final_thickness * n0) /
                                         gpencil_stroke_data.viewport,
                                     getZdepth(P1),
                                     1.0f);
      strip_EmitVertex(0, out_vertex_id, out_primitive_id, geom_out);

      geom_out.tex_coord = float2(0, 0);
      geom_out.final_color = geom_in[1].final_color;
      geom_out.gpu_position = float4((sp1 + geom_in[1].final_thickness * n1) /
                                         gpencil_stroke_data.viewport,
                                     getZdepth(P1),
                                     1.0f);
      strip_EmitVertex(1, out_vertex_id, out_primitive_id, geom_out);

      geom_out.tex_coord = float2(0, 0.5f);
      geom_out.final_color = geom_in[1].final_color;
      geom_out.gpu_position = float4(sp1 / gpencil_stroke_data.viewport, getZdepth(P1), 1.0f);
      strip_EmitVertex(2, out_vertex_id, out_primitive_id, geom_out);
    }
    else {
      geom_out.tex_coord = float2(0, 1);
      geom_out.final_color = geom_in[1].final_color;
      geom_out.gpu_position = float4((sp1 - geom_in[1].final_thickness * n1) /
                                         gpencil_stroke_data.viewport,
                                     getZdepth(P1),
                                     1.0f);
      strip_EmitVertex(0, out_vertex_id, out_primitive_id, geom_out);

      geom_out.tex_coord = float2(0, 1);
      geom_out.final_color = geom_in[1].final_color;
      geom_out.gpu_position = float4((sp1 - geom_in[1].final_thickness * n0) /
                                         gpencil_stroke_data.viewport,
                                     getZdepth(P1),
                                     1.0f);
      strip_EmitVertex(1, out_vertex_id, out_primitive_id, geom_out);

      geom_out.tex_coord = float2(0, 0.5f);
      geom_out.final_color = geom_in[1].final_color;
      geom_out.gpu_position = float4(sp1 / gpencil_stroke_data.viewport, getZdepth(P1), 1.0f);
      strip_EmitVertex(2, out_vertex_id, out_primitive_id, geom_out);
    }

    /* Restart the strip. */
    geom_out.gpu_position = float4(NAN_FLT);
    strip_EmitVertex(3, out_vertex_id, out_primitive_id, geom_out);
  }

  if (dot(v1, v2) < -MiterLimit) {
    miter_b = n1;
    length_b = geom_in[2].final_thickness;
  }

  /* Generate the start end-cap (alpha < 0 used as end-cap flag). */
  float extend = gpencil_stroke_data.fill_stroke ? 2 : 1;
  if ((gpencil_stroke_data.caps_start != GPENCIL_FLATCAP) && is_equal(P0, P2)) {
    geom_out.tex_coord = float2(1, 0.5f);
    geom_out.final_color = float4(geom_in[1].final_color.rgb, geom_in[1].final_color.a * -1.0f);
    float2 svn1 = normalize(sp1 - sp2) * length_a * 4.0f * extend;
    geom_out.gpu_position = float4(
        (sp1 + svn1) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0f);
    strip_EmitVertex(4, out_vertex_id, out_primitive_id, geom_out);

    geom_out.tex_coord = float2(0, 0);
    geom_out.final_color = float4(geom_in[1].final_color.rgb, geom_in[1].final_color.a * -1.0f);
    geom_out.gpu_position = float4(
        (sp1 - (length_a * 2.0f) * miter_a) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0f);
    strip_EmitVertex(5, out_vertex_id, out_primitive_id, geom_out);

    geom_out.tex_coord = float2(0, 1);
    geom_out.final_color = float4(geom_in[1].final_color.rgb, geom_in[1].final_color.a * -1.0f);
    geom_out.gpu_position = float4(
        (sp1 + (length_a * 2.0f) * miter_a) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0f);
    strip_EmitVertex(6, out_vertex_id, out_primitive_id, geom_out);
  }

  /* generate the triangle strip */
  geom_out.tex_coord = float2(0, 0);
  geom_out.final_color = geom_in[1].final_color;
  geom_out.gpu_position = float4(
      (sp1 + length_a * miter_a) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0f);
  strip_EmitVertex(7, out_vertex_id, out_primitive_id, geom_out);

  geom_out.tex_coord = float2(0, 1);
  geom_out.final_color = geom_in[1].final_color;
  geom_out.gpu_position = float4(
      (sp1 - length_a * miter_a) / gpencil_stroke_data.viewport, getZdepth(P1), 1.0f);
  strip_EmitVertex(8, out_vertex_id, out_primitive_id, geom_out);

  geom_out.tex_coord = float2(0, 0);
  geom_out.final_color = geom_in[2].final_color;
  geom_out.gpu_position = float4(
      (sp2 + length_b * miter_b) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0f);
  strip_EmitVertex(9, out_vertex_id, out_primitive_id, geom_out);

  geom_out.tex_coord = float2(0, 1);
  geom_out.final_color = geom_in[2].final_color;
  geom_out.gpu_position = float4(
      (sp2 - length_b * miter_b) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0f);
  strip_EmitVertex(10, out_vertex_id, out_primitive_id, geom_out);

  /* Generate the end end-cap (alpha < 0 used as end-cap flag). */
  if ((gpencil_stroke_data.caps_end != GPENCIL_FLATCAP) && is_equal(P1, P3)) {
    geom_out.tex_coord = float2(0, 1);
    geom_out.final_color = float4(geom_in[2].final_color.rgb, geom_in[2].final_color.a * -1.0f);
    geom_out.gpu_position = float4(
        (sp2 + (length_b * 2.0f) * miter_b) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0f);
    strip_EmitVertex(11, out_vertex_id, out_primitive_id, geom_out);

    geom_out.tex_coord = float2(0, 0);
    geom_out.final_color = float4(geom_in[2].final_color.rgb, geom_in[2].final_color.a * -1.0f);
    geom_out.gpu_position = float4(
        (sp2 - (length_b * 2.0f) * miter_b) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0f);
    strip_EmitVertex(12, out_vertex_id, out_primitive_id, geom_out);

    geom_out.tex_coord = float2(1, 0.5f);
    geom_out.final_color = float4(geom_in[2].final_color.rgb, geom_in[2].final_color.a * -1.0f);
    float2 svn2 = normalize(sp2 - sp1) * length_b * 4.0f * extend;
    geom_out.gpu_position = float4(
        (sp2 + svn2) / gpencil_stroke_data.viewport, getZdepth(P2), 1.0f);
    strip_EmitVertex(13, out_vertex_id, out_primitive_id, geom_out);
  }
}

void main()
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

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(gl_VertexID) % output_primitive_vertex_count;
  uint out_primitive_id = (uint(gl_VertexID) / output_primitive_vertex_count) %
                          output_primitive_count;

  GreasePencilStrokeData vert_in[4];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = input_assembly(in_primitive_first_vertex + 1u);
  vert_in[2] = input_assembly(in_primitive_first_vertex + 2u);
  vert_in[3] = input_assembly(in_primitive_first_vertex + 3u);

  VertOut vert_out[4];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);
  vert_out[2] = vertex_main(vert_in[2]);
  vert_out[3] = vertex_main(vert_in[3]);

  /* Discard by default. */
  gl_Position = float4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id);
}
