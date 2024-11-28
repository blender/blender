/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

struct VertIn {
  vec3 ls_P;
  vec4 final_color;
};

VertIn input_assembly(uint in_vertex_id)
{
  uint v_i = gpu_index_load(in_vertex_id);
  uint ofs = uint(gpu_vert_stride_count_offset.z);

  VertIn vert_in;
  vert_in.ls_P = vec3(0.0, 0.0, 0.0);
  /* Need to support 1, 2 and 3 dimensional input (sigh). */
  vert_in.ls_P.x = pos[gpu_attr_load_index(v_i, gpu_attr_0) + 0 + ofs];
  if (gpu_attr_0_len >= 2) {
    vert_in.ls_P.y = pos[gpu_attr_load_index(v_i, gpu_attr_0) + 1 + ofs];
  }
  if (gpu_attr_0_len >= 3) {
    vert_in.ls_P.z = pos[gpu_attr_load_index(v_i, gpu_attr_0) + 2 + ofs];
  }

  if (gpu_attr_0_fetch_int) {
    vert_in.ls_P = vec3(floatBitsToInt(vert_in.ls_P));
  }
#ifndef UNIFORM
  vert_in.final_color = vec4(0.0, 0.0, 0.0, 1.0);
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
#endif
  return vert_in;
}

struct VertOut {
  vec4 gpu_position;
  vec4 final_color;
  float clip;
};

VertOut vertex_main(VertIn vert_in)
{
  VertOut vert_out;
  vert_out.gpu_position = ModelViewProjectionMatrix * vec4(vert_in.ls_P, 1.0);
#ifndef UNIFORM
  vert_out.final_color = vert_in.final_color;
#endif
#ifdef CLIP
  vert_out.clip = dot(ModelMatrix * vec4(vert_in.ls_P, 1.0), ClipPlane);
#endif
  return vert_out;
}

/* Clips point to near clip plane before perspective divide. */
vec4 clip_line_point_homogeneous_space(vec4 p, vec4 q)
{
  if (p.z < -p.w) {
    /* Just solves p + (q - p) * A; for A when p.z / p.w = -1.0. */
    float denom = q.z - p.z + q.w - p.w;
    if (denom == 0.0) {
      /* No solution. */
      return p;
    }
    float A = (-p.z - p.w) / denom;
    p = p + (q - p) * A;
  }
  return p;
}

struct GeomOut {
  vec4 gpu_position;
  vec4 final_color;
  float clip;
  float smoothline;
};

void export_vertex(GeomOut geom_out)
{
  gl_Position = geom_out.gpu_position;
  final_color = geom_out.final_color;
  smoothline = geom_out.smoothline;
  clip = geom_out.clip;
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

void do_vertex(const uint i,
               uint out_vertex_id,
               uint out_primitive_id,
               VertOut geom_in[2],
               vec4 position,
               vec2 ofs)
{
  GeomOut geom_out;
#if defined(UNIFORM)
  geom_out.final_color = color;

#elif defined(FLAT)
  /* WATCH: Assuming last provoking vertex. */
  geom_out.final_color = geom_in[1].final_color;

#elif defined(SMOOTH)
  geom_out.final_color = geom_in[i].final_color;
#endif

#ifdef CLIP
  geom_out.clip = geom_in[i].clip;
#endif

  geom_out.smoothline = (lineWidth + SMOOTH_WIDTH * float(lineSmooth)) * 0.5;
  geom_out.gpu_position = position;
  geom_out.gpu_position.xy += ofs * position.w;
  strip_EmitVertex(i * 2u + 0u, out_vertex_id, out_primitive_id, geom_out);

  geom_out.smoothline = -(lineWidth + SMOOTH_WIDTH * float(lineSmooth)) * 0.5;
  geom_out.gpu_position = position;
  geom_out.gpu_position.xy -= ofs * position.w;
  strip_EmitVertex(i * 2u + 1u, out_vertex_id, out_primitive_id, geom_out);
}

void geometry_main(VertOut geom_in[2],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  vec4 p0 = clip_line_point_homogeneous_space(geom_in[0].gpu_position, geom_in[1].gpu_position);
  vec4 p1 = clip_line_point_homogeneous_space(geom_in[1].gpu_position, geom_in[0].gpu_position);
  vec2 e = normalize(((p1.xy / p1.w) - (p0.xy / p0.w)) * viewportSize.xy);

#if 0 /* Hard turn when line direction changes quadrant. */
  e = abs(e);
  vec2 ofs = (e.x > e.y) ? vec2(0.0, 1.0 / e.x) : vec2(1.0 / e.y, 0.0);
#else /* Use perpendicular direction. */
  vec2 ofs = vec2(-e.y, e.x);
#endif
  ofs /= viewportSize.xy;
  ofs *= lineWidth + SMOOTH_WIDTH * float(lineSmooth);

  do_vertex(0u, out_vertex_id, out_primitive_id, geom_in, p0, ofs);
  do_vertex(1u, out_vertex_id, out_primitive_id, geom_in, p1, ofs);
}

void main()
{
  /* Line list primitive. */
  uint input_primitive_vertex_count = uint(gpu_vert_stride_count_offset.x);
  /* Triangle list primitive (emulating triangle strip). */
  const uint ouput_primitive_vertex_count = 3u;
  const uint ouput_primitive_count = 2u;
  const uint ouput_invocation_count = 1u;
  const uint output_vertex_count_per_invocation = ouput_primitive_count *
                                                  ouput_primitive_vertex_count;
  const uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                       ouput_invocation_count;

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(gl_VertexID) % ouput_primitive_vertex_count;
  uint out_primitive_id = (uint(gl_VertexID) / ouput_primitive_vertex_count) %
                          ouput_primitive_count;
  uint out_invocation_id = (uint(gl_VertexID) / output_vertex_count_per_invocation) %
                           ouput_invocation_count;
  /* Used to wrap around for the line loop case. */
  uint input_total_vertex_count = uint(gpu_vert_stride_count_offset.y);

  VertIn vert_in[2];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = input_assembly((in_primitive_first_vertex + 1u) % input_total_vertex_count);

  VertOut vert_out[2];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);

  /* Discard by default. */
  gl_Position = vec4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
