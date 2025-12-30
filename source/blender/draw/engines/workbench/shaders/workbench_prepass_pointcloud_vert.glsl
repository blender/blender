/* SPDX-FileCopyrightText: 2016-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_prepass_infos.hh"

VERTEX_SHADER_CREATE_INFO(workbench_prepass)
VERTEX_SHADER_CREATE_INFO(workbench_lighting_flat)
VERTEX_SHADER_CREATE_INFO(workbench_transparent_accum)
VERTEX_SHADER_CREATE_INFO(workbench_pointcloud)

#include "draw_view_lib.glsl"

#include "draw_pointcloud_lib.glsl"
#include "draw_view_clipping_lib.glsl"

#include "workbench_common_lib.glsl"
#include "workbench_image_lib.glsl"
#include "workbench_material_lib.glsl"

void main()
{
  const pointcloud::Point ls_pt = pointcloud::point_get(uint(gl_VertexID));
  const pointcloud::Point ws_pt = pointcloud::object_to_world(ls_pt, drw_modelmat());
  const pointcloud::ShapePoint pt = pointcloud::shape_point_get(
      ws_pt, drw_world_incident_vector(ws_pt.P), drw_view_up());

  normal_interp = normalize(drw_normal_world_to_view(pt.N));

  gl_Position = drw_point_world_to_homogenous(pt.P);

  view_clipping_distances(pt.P);

  uv_interp = float2(0.0f);

  workbench_material_data_get(
      int(drw_custom_id()), float3(1.0f), color_interp, alpha_interp, _roughness, metallic);

  object_id = int(uint(drw_resource_id()) & 0xFFFFu) + 1;
}
