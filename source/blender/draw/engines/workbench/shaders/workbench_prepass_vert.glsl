
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_material_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  view_clipping_distances(world_pos);

  uv_interp = au;

  normal_interp = normalize(normal_object_to_view(nor));

  workbench_material_data_get(
      resource_handle, ac.rgb, color_interp, alpha_interp, _roughness, metallic);

  object_id = int(uint(resource_handle) & 0xFFFFu) + 1;
}
