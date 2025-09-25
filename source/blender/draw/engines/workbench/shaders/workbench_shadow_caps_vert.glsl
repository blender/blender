/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Render shadow casters extrusion caps.
 * Manifold meshes generate caps for triangles facing the light.
 * Non-Manifold meshes generate caps for all triangles and invert the winding for back-facing ones.
 *
 * This shader uses triangle primitive to know the geometric normals of a triangle.
 * Two triangles are generated for each original triangle facing the light. One in front of the
 * shadow volume and one at the back with reversed winding to decrement the stencil buffer.
 *
 * This vertex shader emulates a geometry shader. The draw call generate enough triangle for one or
 * two quads per input primitive. Each vertex shader invocation reads the whole input primitive and
 * execute the vertex shader code on each of the input primitive's vertices.
 */

#include "infos/workbench_shadow_infos.hh"

VERTEX_SHADER_CREATE_INFO(workbench_shadow_common)

#include "workbench_shadow_lib.glsl"

void emit_cap(bool front,
              bool invert,
              VertOut geom_in_0,
              VertOut geom_in_1,
              VertOut geom_in_2,
              uint out_vertex_id)
{
  if (invert == front) {
    VertOut geom_in_tmp = geom_in_1;
    geom_in_1 = geom_in_2;
    geom_in_2 = geom_in_tmp;
  }

  GeomOut geom_out;
  geom_out.gpu_position = front ? geom_in_0.frontPosition : geom_in_0.backPosition;
  tri_EmitVertex(0, out_vertex_id, geom_out);

  geom_out.gpu_position = front ? geom_in_1.frontPosition : geom_in_1.backPosition;
  tri_EmitVertex(1, out_vertex_id, geom_out);

  geom_out.gpu_position = front ? geom_in_2.frontPosition : geom_in_2.backPosition;
  tri_EmitVertex(2, out_vertex_id, geom_out);
}

void geometry_main(VertOut geom_in[3], uint out_vertex_id, uint out_invocation_id)
{
  float3 v10 = geom_in[0].lP - geom_in[1].lP;
  float3 v12 = geom_in[2].lP - geom_in[1].lP;

  float3 Ng = cross(v12, v10);

  float3 ls_light_direction = drw_normal_world_to_object(float3(pass_data.light_direction_ws));

  float facing = dot(Ng, ls_light_direction);

  bool backface = facing > 0.0f;

#ifdef DOUBLE_MANIFOLD
  /* In case of non manifold geom, we only increase/decrease
   * the stencil buffer by one but do every faces as they were facing the light. */
  bool invert = backface;
  constexpr bool is_manifold = false;
#else
  constexpr bool invert = false;
  constexpr bool is_manifold = true;
#endif

  if (!is_manifold || !backface) {
    bool do_front = out_invocation_id == 0;
    emit_cap(do_front, invert, geom_in[0], geom_in[1], geom_in[2], out_vertex_id);
  }
}

void main()
{
  /* Triangle list primitive. */
  constexpr uint input_primitive_vertex_count = 3u;
  /* Triangle list primitive. */
  constexpr uint output_primitive_vertex_count = 3u;
  constexpr uint output_primitive_count = 1u;
  constexpr uint output_invocation_count = 2u;

  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(gl_VertexID) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(gl_VertexID) % output_primitive_vertex_count;
  uint out_invocation_id = (uint(gl_VertexID) / output_vertex_count_per_invocation) %
                           output_invocation_count;

  VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = input_assembly(in_primitive_first_vertex + 0u);
  vert_in[1] = input_assembly(in_primitive_first_vertex + 1u);
  vert_in[2] = input_assembly(in_primitive_first_vertex + 2u);

  VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);
  vert_out[2] = vertex_main(vert_in[2]);

  /* Discard by default. */
  gl_Position = float4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_invocation_id);
}
