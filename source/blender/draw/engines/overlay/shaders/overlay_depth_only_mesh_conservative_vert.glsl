/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_index_load_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "select_lib.glsl"

struct VertIn {
  vec3 ls_P;
};

VertIn input_assembly(uint in_vertex_id)
{
  uint v_i = gpu_index_load(in_vertex_id);

  VertIn vert_in;
  vert_in.ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
  return vert_in;
}

struct VertOut {
  vec4 hs_P;
  vec3 ws_P;
};

VertOut vertex_main(VertIn v_in)
{
  VertOut v_out;
  v_out.ws_P = drw_point_object_to_world(v_in.ls_P);
  v_out.hs_P = drw_point_world_to_homogenous(v_out.ws_P);

  return v_out;
}

void do_vertex(const uint i,
               uint out_vertex_id,
               uint out_primitive_id,
               VertOut geom_in,
               bvec2 is_subpixel,
               bool is_coplanar)
{
  if (out_vertex_id != i) {
    return;
  }

  view_clipping_distances(geom_in.ws_P);

  gl_Position = geom_in.hs_P;
  if (all(is_subpixel)) {
    vec2 ofs = (i == 0) ? vec2(-1.0) : ((i == 1) ? vec2(2.0, -1.0) : vec2(-1.0, 2.0));
    /* HACK: Fix cases where the triangle is too small make it cover at least one pixel. */
    gl_Position.xy += sizeViewportInv * geom_in.hs_P.w * ofs;
  }
  /* Test if the triangle is almost parallel with the view to avoid precision issues. */
  else if (any(is_subpixel) || is_coplanar) {
    /* HACK: Fix cases where the triangle is Parallel to the view by deforming it slightly. */
    vec2 ofs = (i == 0) ? vec2(-1.0) : ((i == 1) ? vec2(1.0, -1.0) : vec2(1.0));
    gl_Position.xy += sizeViewportInv * geom_in.hs_P.w * ofs;
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
  vec3 pos0 = geom_in[0].hs_P.xyz / geom_in[0].hs_P.w;
  vec3 pos1 = geom_in[1].hs_P.xyz / geom_in[1].hs_P.w;
  vec3 pos2 = geom_in[2].hs_P.xyz / geom_in[2].hs_P.w;
  vec3 plane = normalize(cross(pos1 - pos0, pos2 - pos0));
  /* Compute NDC bound box. */
  vec4 bbox = vec4(min(min(pos0.xy, pos1.xy), pos2.xy), max(max(pos0.xy, pos1.xy), pos2.xy));
  /* Convert to pixel space. */
  bbox = (bbox * 0.5 + 0.5) * sizeViewport.xyxy;
  /* Detect failure cases where triangles would produce no fragments. */
  bvec2 is_subpixel = lessThan(bbox.zw - bbox.xy, vec2(1.0));
  /* View aligned triangle. */
  const float threshold = 0.00001;
  bool is_coplanar = abs(plane.z) < threshold;

  do_vertex(0, out_vertex_id, out_primitive_id, geom_in[0], is_subpixel, is_coplanar);
  do_vertex(1, out_vertex_id, out_primitive_id, geom_in[1], is_subpixel, is_coplanar);
  do_vertex(2, out_vertex_id, out_primitive_id, geom_in[2], is_subpixel, is_coplanar);
}

void main()
{
  select_id_set(drw_CustomID);

  /* Triangle list primitive. */
  const uint input_primitive_vertex_count = 3u;
  /* Triangle list primitive. */
  const uint ouput_primitive_vertex_count = 3u;
  const uint ouput_primitive_count = 1u;
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
  vert_in[2] = input_assembly(in_primitive_first_vertex + 2u);

  VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = vertex_main(vert_in[0]);
  vert_out[1] = vertex_main(vert_in[1]);
  vert_out[2] = vertex_main(vert_in[2]);

  /* Discard by default. */
  gl_Position = vec4(NAN_FLT);
  geometry_main(vert_out, out_vertex_id, out_primitive_id, out_invocation_id);
}
