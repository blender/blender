/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

struct VertIn {
  vec3 lP;
  mat4 inst_matrix;
};

VertIn input_assembly(uint in_vertex_id, mat4x4 inst_matrix)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.lP = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  vert_in.inst_matrix = inst_matrix;
  return vert_in;
}

struct VertOut {
  vec4 gpu_position;
  vec4 finalColor;
  vec3 world_pos;
  float wire_width;
};

VertOut vertex_main(VertIn v_in)
{
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(v_in.inst_matrix, state_color, bone_color);

  VertOut v_out;

  /* WORKAROUND: This shape needs a special vertex shader path that should be triggered by
   * its `vclass` attribute. However, to avoid many changes in the primitive expansion API,
   * we create a specific path inside the shader only for this shape batch and infer the
   * value of the `vclass` attribute based on the vertex index. */
  if (use_arrow_drawing) {
    /* Keep in sync with the arrows shape batch creation. */
    /* Adapted from `overlay_extra_vert.glsl`. */
    vec3 vpos = v_in.lP;
    vec3 vofs = vec3(0.0);
    uint axis = uint(vpos.z);
    /* Assumes origin vertices are the only one at Z=0. */
    if (vpos.z > 0.0) {
      vofs[axis] = (1.0 + fract(vpos.z));
    }
    /* Scale uniformly by axis length */
    vpos *= length(model_mat[axis].xyz);
    /* World sized, camera facing geometry. */
    vec3 screen_pos = ViewMatrixInverse[0].xyz * vpos.x + ViewMatrixInverse[1].xyz * vpos.y;
    v_out.world_pos = (model_mat * vec4(vofs, 1.0)).xyz + screen_pos;
  }
  else {
    v_out.world_pos = (model_mat * vec4(v_in.lP, 1.0)).xyz;
  }
  v_out.gpu_position = drw_point_world_to_homogenous(v_out.world_pos);

  v_out.finalColor.rgb = mix(state_color.rgb, bone_color.rgb, 0.5);
  v_out.finalColor.a = 1.0;
  /* Because the packing clamps the value, the wire width is passed in compressed. */
  v_out.wire_width = bone_color.a * WIRE_WIDTH_COMPRESSION;

  return v_out;
}

void do_vertex(const uint strip_index,
               uint out_vertex_id,
               uint out_primitive_id,
               vec4 color,
               vec4 hs_P,
               vec3 ws_P,
               float coord,
               vec2 offset)
{
  bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
  /* Maps triangle list primitives to triangle strip indices. */
  uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                         out_primitive_id;

  if (out_strip_index != strip_index) {
    return;
  }

  finalColor = color;
  edgeCoord = coord;
  gl_Position = hs_P;
  /* Multiply offset by 2 because gl_Position range is [-1..1]. */
  gl_Position.xy += offset * 2.0 * hs_P.w;

  view_clipping_distances(ws_P);
}

void geometry_main(VertOut geom_in[2],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  /* Clip line against near plane to avoid deformed lines. */
  vec4 pos0 = geom_in[0].gpu_position;
  vec4 pos1 = geom_in[1].gpu_position;
  vec2 pz_ndc = vec2(pos0.z / pos0.w, pos1.z / pos1.w);
  bvec2 clipped = lessThan(pz_ndc, vec2(-1.0));
  if (all(clipped)) {
    /* Totally clipped. */
    return;
  }

  vec4 pos01 = pos0 - pos1;
  float ofs = abs((pz_ndc.y + 1.0) / (pz_ndc.x - pz_ndc.y));
  if (clipped.y) {
    pos1 += pos01 * ofs;
  }
  else if (clipped.x) {
    pos0 -= pos01 * (1.0 - ofs);
  }

  vec2 screen_space_pos[2];
  screen_space_pos[0] = pos0.xy / pos0.w;
  screen_space_pos[1] = pos1.xy / pos1.w;

  /* `sizeEdge` is defined as the distance from the center to the outer edge. As such to get the
   total width it needs to be doubled. */
  wire_width = geom_in[0].wire_width * (sizeEdge * 2);
  float half_size = max(wire_width / 2.0, 0.5);

  if (do_smooth_wire) {
    /* Add 1px for AA */
    half_size += 0.5;
  }

  vec2 line = (screen_space_pos[0] - screen_space_pos[1]) * sizeViewport.xy;
  vec2 line_norm = normalize(vec2(line[1], -line[0]));
  vec2 edge_ofs = (half_size * line_norm) * sizeViewportInv;

  vec4 final_color = geom_in[0].finalColor;
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
  const uint input_primitive_vertex_count = 1u;
#else
  const uint input_primitive_vertex_count = 2u;
#endif
  /* Triangle list primitive. */
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

  mat4 inst_obmat = data_buf[gl_InstanceID];
  mat4x4 inst_matrix = inst_obmat;

#ifdef FROM_LINE_STRIP
  uint32_t RESTART_INDEX = gpu_index_16bit ? 0xFFFF : 0xFFFFFFFF;
  if (gpu_index_load(in_primitive_first_vertex + 0u) == RESTART_INDEX ||
      gpu_index_load(in_primitive_first_vertex + 1u) == RESTART_INDEX)
  {
    /* Discard. */
    gl_Position = vec4(NAN_FLT);
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
  gl_Position = vec4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
