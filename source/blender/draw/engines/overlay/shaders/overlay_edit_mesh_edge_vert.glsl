/* SPDX-FileCopyrightText: 2017-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_edit_mesh_common_lib.glsl"
#include "overlay_edit_mesh_lib.glsl"

VertIn input_assembly(uint in_vertex_id)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.lP = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  if (gpu_attr_1.x == 1) {
    vert_in.lN = gpu_attr_load_uint_1010102_snorm(vnor, gpu_attr_1, v_i).xyz;
  }
  else {
    vert_in.lN.x = uintBitsToFloat(vnor[gpu_attr_load_index(v_i, gpu_attr_1) + 0]);
    vert_in.lN.y = uintBitsToFloat(vnor[gpu_attr_load_index(v_i, gpu_attr_1) + 1]);
    vert_in.lN.z = uintBitsToFloat(vnor[gpu_attr_load_index(v_i, gpu_attr_1) + 2]);
  }
  vert_in.e_data = gpu_attr_load_uchar4(data, gpu_attr_2, v_i);
  return vert_in;
}

struct GeomOut {
  vec4 gpu_position;
  vec4 final_color;
  vec3 world_pos;
  float edge_coord;
};

void export_vertex(GeomOut geom_out)
{
  geometry_out.finalColor = geom_out.final_color;
  geometry_noperspective_out.edgeCoord = geom_out.edge_coord;
  view_clipping_distances(geom_out.world_pos);
  gl_Position = geom_out.gpu_position;
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

void do_vertex(const uint strip_index,
               uint out_vertex_id,
               uint out_primitive_id,
               vec4 final_color,
               vec4 ndc_position,
               vec3 world_position,
               float coord,
               vec2 offset)
{
  GeomOut geom_out;
  geom_out.world_pos = world_position;
  geom_out.final_color = final_color;
  geom_out.edge_coord = coord;
  geom_out.gpu_position = ndc_position;
  /* Multiply offset by 2 because gl_Position range is [-1..1]. */
  geom_out.gpu_position.xy += offset * 2.0 * ndc_position.w;
  strip_EmitVertex(strip_index, out_vertex_id, out_primitive_id, geom_out);
}

void geometry_main(VertOut geom_in[2], uint out_vert_id, uint out_prim_id, uint out_invocation_id)
{
  vec2 ss_pos[2];

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

  ss_pos[0] = pos0.xy / pos0.w;
  ss_pos[1] = pos1.xy / pos1.w;

  vec2 line = ss_pos[0] - ss_pos[1];
  line = abs(line) * sizeViewport.xy;

  geometry_flat_out.finalColorOuter = geom_in[0].final_color_outer;
  float half_size = sizeEdge;
  /* Enlarge edge for flag display. */
  half_size += (geometry_flat_out.finalColorOuter.a > 0.0) ? max(sizeEdge, 1.0) : 0.0;

  if (do_smooth_wire) {
    /* Add 1px for AA */
    half_size += 0.5;
  }

  vec3 edge_ofs = vec3(half_size * sizeViewportInv, 0.0);

  bool horizontal = line.x > line.y;
  edge_ofs = (horizontal) ? edge_ofs.zyz : edge_ofs.xzz;

  vec3 wpos0 = geom_in[0].world_position;
  vec3 wpos1 = geom_in[1].world_position;

  vec4 final_color1 = geom_in[0].final_color;
  vec4 final_color2 = (geom_in[0].select_override == 0u) ? geom_in[1].final_color :
                                                           geom_in[0].final_color;

  do_vertex(0, out_vert_id, out_prim_id, final_color1, pos0, wpos0, half_size, edge_ofs.xy);
  do_vertex(1, out_vert_id, out_prim_id, final_color1, pos0, wpos0, -half_size, -edge_ofs.xy);

  do_vertex(2, out_vert_id, out_prim_id, final_color2, pos1, wpos1, half_size, edge_ofs.xy);
  do_vertex(3, out_vert_id, out_prim_id, final_color2, pos1, wpos1, -half_size, -edge_ofs.xy);
}

void main()
{
  /* Line list primitive. */
  const uint input_primitive_vertex_count = 2u;
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

  VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = input_assembly(in_primitive_first_vertex + 1u);

  VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);

  /* Discard by default. */
  gl_Position = vec4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
