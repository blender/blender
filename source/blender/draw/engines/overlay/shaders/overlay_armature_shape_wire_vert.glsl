/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_armature_shape_wire)

#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

struct VertIn {
  float3 lP;
  float4x4 inst_matrix;
};

VertIn input_assembly(uint in_vertex_id, float4x4 inst_matrix)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.lP = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  vert_in.inst_matrix = inst_matrix;
  return vert_in;
}

struct VertOut {
  float4 gpu_position;
  float4 final_color;
  float3 world_pos;
  float wire_width;
};

VertOut vertex_main(VertIn v_in)
{
  float4 bone_color, state_color;
  float4x4 model_mat = extract_matrix_packed_data(v_in.inst_matrix, state_color, bone_color);

  VertOut v_out;

  /* WORKAROUND: This shape needs a special vertex shader path that should be triggered by
   * its `vclass` attribute. However, to avoid many changes in the primitive expansion API,
   * we create a specific path inside the shader only for this shape batch and infer the
   * value of the `vclass` attribute based on the vertex index. */
  if (use_arrow_drawing) {
    /* Keep in sync with the arrows shape batch creation. */
    /* Adapted from `overlay_extra_vert.glsl`. */
    float3 vpos = v_in.lP;
    float3 vofs = float3(0.0f);
    uint axis = uint(vpos.z);
    /* Assumes origin vertices are the only one at Z=0. */
    if (vpos.z > 0.0f) {
      vofs[axis] = (1.0f + fract(vpos.z));
    }
    /* Scale uniformly by axis length */
    vpos *= length(model_mat[axis].xyz);
    /* World sized, camera facing geometry. */
    float3 screen_pos = drw_view().viewinv[0].xyz * vpos.x + drw_view().viewinv[1].xyz * vpos.y;
    v_out.world_pos = (model_mat * float4(vofs, 1.0f)).xyz + screen_pos;
  }
  else {
    v_out.world_pos = (model_mat * float4(v_in.lP, 1.0f)).xyz;
  }
  v_out.gpu_position = drw_point_world_to_homogenous(v_out.world_pos);

  v_out.final_color.rgb = mix(state_color.rgb, bone_color.rgb, 0.5f);
  v_out.final_color.a = 1.0f;
  /* Because the packing clamps the value, the wire width is passed in compressed. */
  v_out.wire_width = bone_color.a * WIRE_WIDTH_COMPRESSION;

  return v_out;
}

void do_vertex(const uint strip_index,
               uint out_vertex_id,
               uint out_primitive_id,
               float4 color,
               float4 hs_P,
               float3 ws_P,
               float coord,
               float2 offset)
{
  bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
  /* Maps triangle list primitives to triangle strip indices. */
  uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                         out_primitive_id;

  if (out_strip_index != strip_index) {
    return;
  }

  final_color = color;
  edge_coord = coord;
  gl_Position = hs_P;
  /* Multiply offset by 2 because gl_Position range is [-1..1]. */
  gl_Position.xy += offset * 2.0f * hs_P.w;

  view_clipping_distances(ws_P);
}

void geometry_main(VertOut geom_in[2],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  /* Clip line against near plane to avoid deformed lines. */
  float4 pos0 = geom_in[0].gpu_position;
  float4 pos1 = geom_in[1].gpu_position;
  float2 pz_ndc = float2(pos0.z / pos0.w, pos1.z / pos1.w);
  bool2 clipped = lessThan(pz_ndc, float2(-1.0f));
  if (all(clipped)) {
    /* Totally clipped. */
    return;
  }

  float4 pos01 = pos0 - pos1;
  float ofs = abs((pz_ndc.y + 1.0f) / (pz_ndc.x - pz_ndc.y));
  if (clipped.y) {
    pos1 += pos01 * ofs;
  }
  else if (clipped.x) {
    pos0 -= pos01 * (1.0f - ofs);
  }

  float2 screen_space_pos[2];
  screen_space_pos[0] = pos0.xy / pos0.w;
  screen_space_pos[1] = pos1.xy / pos1.w;

  /* `theme.sizes.edge` is defined as the distance from the center to the outer edge.
   * As such to get the total width it needs to be doubled. */
  wire_width = geom_in[0].wire_width * (theme.sizes.edge * 2);
  float half_size = max(wire_width / 2.0f, 0.5f);

  if (do_smooth_wire) {
    /* Add 1px for AA */
    half_size += 0.5f;
  }

  float2 line = (screen_space_pos[0] - screen_space_pos[1]) * uniform_buf.size_viewport;
  float2 line_norm = normalize(float2(line[1], -line[0]));
  float2 edge_ofs = (half_size * line_norm) * uniform_buf.size_viewport_inv;

  float4 final_color = geom_in[0].final_color;
  do_vertex(0,
            out_vertex_id,
            out_primitive_id,
            final_color,
            pos0,
            geom_in[0].world_pos,
            half_size,
            edge_ofs);
  do_vertex(1,
            out_vertex_id,
            out_primitive_id,
            final_color,
            pos0,
            geom_in[0].world_pos,
            -half_size,
            -edge_ofs);

  do_vertex(2,
            out_vertex_id,
            out_primitive_id,
            final_color,
            pos1,
            geom_in[1].world_pos,
            half_size,
            edge_ofs);
  do_vertex(3,
            out_vertex_id,
            out_primitive_id,
            final_color,
            pos1,
            geom_in[1].world_pos,
            -half_size,
            -edge_ofs);
}

void main()
{
  select_id_set(in_select_buf[gl_InstanceID]);

/* Line primitive. */
#ifdef FROM_LINE_STRIP
  constexpr uint input_primitive_vertex_count = 1u;
#else
  constexpr uint input_primitive_vertex_count = 2u;
#endif
  /* Triangle list primitive. */
  constexpr uint output_primitive_vertex_count = 3u;
  constexpr uint output_primitive_count = 2u;
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
  uint out_invocation_id = (uint(gl_VertexID) / output_vertex_count_per_invocation) %
                           output_invocation_count;

  float4x4 inst_obmat = data_buf[gl_InstanceID];
  float4x4 inst_matrix = inst_obmat;

#ifdef FROM_LINE_STRIP
  uint32_t RESTART_INDEX = gpu_index_16bit ? 0xFFFF : 0xFFFFFFFF;
  if (gpu_index_load(in_primitive_first_vertex + 0u) == RESTART_INDEX ||
      gpu_index_load(in_primitive_first_vertex + 1u) == RESTART_INDEX)
  {
    /* Discard. */
    gl_Position = float4(NAN_FLT);
    return;
  }
#endif

  VertIn vert_in[2];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u, inst_matrix);
  vert_in[1] = input_assembly(in_primitive_first_vertex + 1u, inst_matrix);

  VertOut vert_out[2];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);

  /* Discard by default. */
  gl_Position = float4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
