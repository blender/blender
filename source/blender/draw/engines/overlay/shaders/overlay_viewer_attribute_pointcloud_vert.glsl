/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_viewer_attribute_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_viewer_attribute_pointcloud)

#include "draw_model_lib.glsl"
#include "draw_pointcloud_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  const pointcloud::Point ls_pt = pointcloud::point_get(uint(gl_VertexID));
  const pointcloud::Point ws_pt = pointcloud::object_to_world(ls_pt, drw_modelmat());
  const pointcloud::ShapePoint pt = pointcloud::shape_point_get(
      ws_pt, drw_world_incident_vector(ws_pt.P), drw_view_up());

  gl_Position = drw_point_world_to_homogenous(pt.P);
  final_color = pointcloud::get_customdata_vec4(ws_pt.point_id, attribute_tx);
}
