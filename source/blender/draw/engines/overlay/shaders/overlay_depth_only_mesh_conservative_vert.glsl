/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_depth_mesh_conservative)

#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"

#include "gpu_shader_math_matrix_compare_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "select_lib.glsl"

struct VertIn {
  float3 ls_P;
};

VertIn input_assembly(uint in_vertex_id)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  return vert_in;
}

struct VertOut {
  float4 hs_P;
  float3 ws_P;
};

VertOut vertex_main(VertIn v_in)
{
  VertOut v_out;
  v_out.ws_P = drw_point_object_to_world(v_in.ls_P);
  v_out.hs_P = drw_point_world_to_homogenous(v_out.ws_P);

  return v_out;
}

void do_vertex(uint i,
               uint out_vertex_id,
               uint out_primitive_id,
               VertOut geom_in,
               bool2 is_subpixel,
               bool is_coplanar)
{
  if (out_vertex_id != i) {
    return;
  }

  view_clipping_distances(geom_in.ws_P);

  /* WORKAROUND: The subpixel hack that does the small triangle expansion needs to have correct
   * winding w.r.t. the culling mode. Otherwise, the fragment shader will discard valid triangles
   * and objects will become unselectable (see #85015). */
  if ((any(is_subpixel) || is_coplanar) && is_negative(drw_modelmat())) {
    if (i == 1) {
      i = 2;
    }
    else if (i == 2) {
      i = 1;
    }
  }

  gl_Position = geom_in.hs_P;
  if (all(is_subpixel)) {
    float2 ofs = (i == 0) ? float2(-1.0f) : ((i == 1) ? float2(2.0f, -1.0f) : float2(-1.0f, 2.0f));
    /* HACK: Fix cases where the triangle is too small make it cover at least one pixel. */
    gl_Position.xy += uniform_buf.size_viewport_inv * geom_in.hs_P.w * ofs;
  }
  /* Test if the triangle is almost parallel with the view to avoid precision issues. */
  else if (any(is_subpixel) || is_coplanar) {
    /* HACK: Fix cases where the triangle is Parallel to the view by deforming it slightly. */
    float2 ofs = (i == 0) ? float2(-1.0f) : ((i == 1) ? float2(1.0f, -1.0f) : float2(1.0f));
    gl_Position.xy += uniform_buf.size_viewport_inv * geom_in.hs_P.w * ofs;
  }
  else {
    /* Triangle expansion should happen here, but we decide to not implement it for
     * depth precision & performance reasons. */
  }
}

void geometry_main(VertOut geom_in[3],
                   uint out_vertex_id,
                   uint out_primitive_id,
                   uint out_invocation_id)
{
  /* Compute plane normal in NDC space. */
  float3 pos0 = geom_in[0].hs_P.xyz / geom_in[0].hs_P.w;
  float3 pos1 = geom_in[1].hs_P.xyz / geom_in[1].hs_P.w;
  float3 pos2 = geom_in[2].hs_P.xyz / geom_in[2].hs_P.w;
  float3 plane = normalize(cross(pos1 - pos0, pos2 - pos0));
  /* Compute NDC bound box. */
  float4 bbox = float4(min(min(pos0.xy, pos1.xy), pos2.xy), max(max(pos0.xy, pos1.xy), pos2.xy));
  /* Convert to pixel space. */
  bbox = (bbox * 0.5f + 0.5f) * uniform_buf.size_viewport.xyxy;
  /* Detect failure cases where triangles would produce no fragments. */
  bool2 is_subpixel = lessThan(bbox.zw - bbox.xy, float2(1.0f));
  /* View aligned triangle. */
  constexpr float threshold = 0.00001f;
  bool is_coplanar = abs(plane.z) < threshold;

  do_vertex(0, out_vertex_id, out_primitive_id, geom_in[0], is_subpixel, is_coplanar);
  do_vertex(1, out_vertex_id, out_primitive_id, geom_in[1], is_subpixel, is_coplanar);
  do_vertex(2, out_vertex_id, out_primitive_id, geom_in[2], is_subpixel, is_coplanar);
}

void main()
{
  select_id_set(drw_custom_id());

  /* Triangle list primitive. */
  constexpr uint input_primitive_vertex_count = 3u;
  /* Triangle list primitive. */
  constexpr uint output_primitive_vertex_count = 3u;
  constexpr uint output_primitive_count = 1u;
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
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
