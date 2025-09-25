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
  float3 world_pos = pointcloud_get_pos();
  gl_Position = drw_point_world_to_homogenous(world_pos);
  final_color = pointcloud_get_customdata_vec4(attribute_tx);
}
