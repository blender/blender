/* SPDX-FileCopyrightText: 2016-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_pointcloud_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_material_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)

void main()
{
  vec3 world_pos;
  pointcloud_get_pos_and_nor(world_pos, normal_interp);

  normal_interp = normalize(normal_world_to_view(normal_interp));

  gl_Position = point_world_to_ndc(world_pos);

  view_clipping_distances(world_pos);

  uv_interp = vec2(0.0);

#ifdef WORKBENCH_NEXT
  workbench_material_data_get(
      int(drw_CustomID), vec3(1.0), color_interp, alpha_interp, _roughness, metallic);
#else
  workbench_material_data_get(
      resource_handle, vec3(1.0), color_interp, alpha_interp, _roughness, metallic);
#endif

  object_id = int(uint(resource_handle) & 0xFFFFu) + 1;
}
