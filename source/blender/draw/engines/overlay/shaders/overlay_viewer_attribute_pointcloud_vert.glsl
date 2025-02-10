/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_pointcloud_lib.glsl"
#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"

void main()
{
  vec3 world_pos = pointcloud_get_pos();
  gl_Position = drw_point_world_to_homogenous(world_pos);
  finalColor = pointcloud_get_customdata_vec4(attribute_tx);
}
