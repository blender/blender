/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_edit_mode_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_depth_pointcloud)

#include "draw_model_lib.glsl"
#include "draw_pointcloud_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

void main()
{
  select_id_set(drw_custom_id());

  const pointcloud::Point ls_pt = pointcloud::point_get(uint(gl_VertexID));
  const pointcloud::Point ws_pt = pointcloud::object_to_world(ls_pt, drw_modelmat());
  const pointcloud::ShapePoint pt = pointcloud::shape_point_get(
      ws_pt, drw_world_incident_vector(ws_pt.P), drw_view_up());

  gl_Position = drw_point_world_to_homogenous(pt.P);

#ifdef CONSERVATIVE_RASTER
  /* Avoid expense of geometry shader by ensuring rastered point-cloud primitive
   * covers at least a whole pixel. */
  int i = gl_VertexID % 3;
  float2 ofs = (i == 0) ? float2(-1.0f) : ((i == 1) ? float2(2.0f, -1.0f) : float2(-1.0f, 2.0f));
  gl_Position.xy += uniform_buf.size_viewport_inv * gl_Position.w * ofs;
#endif

  view_clipping_distances(pt.P);
}
