/* SPDX-FileCopyrightText: 2016-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "workbench_common_lib.glsl"
#include "workbench_image_lib.glsl"
#include "workbench_material_lib.glsl"

void main()
{
  vec3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);

  view_clipping_distances(world_pos);

  uv_interp = au;

  normal_interp = normalize(normal_object_to_view(nor));

  object_id = int(uint(resource_id) & 0xFFFFu) + 1;

  workbench_material_data_get(
      int(drw_CustomID), ac.rgb, color_interp, alpha_interp, _roughness, metallic);
}
