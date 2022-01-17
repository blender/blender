
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_pointcloud_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_shader_interface_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_material_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)

void main()
{
  vec3 world_pos;
  pointcloud_get_pos_and_nor(world_pos, normal_interp);

  normal_interp = normalize(normal_world_to_view(normal_interp));

  gl_Position = point_world_to_ndc(world_pos);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif

  uv_interp = vec2(0.0);

#ifndef WORKBENCH_SHADER_SHARED_H
#  ifdef OPAQUE_MATERIAL
  float metallic, roughness;
#  endif
#endif
  workbench_material_data_get(resource_handle, color_interp, alpha_interp, roughness, metallic);

  if (materialIndex == 0) {
    color_interp = vec3(1.0);
  }

#ifdef OPAQUE_MATERIAL
  packed_rough_metal = workbench_float_pair_encode(roughness, metallic);
#endif

  object_id = int(uint(resource_handle) & 0xFFFFu) + 1;
}
