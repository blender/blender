#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  faceset_color = mix(vec3(1.0), fset, faceSetsOpacity);
  mask_color = 1.0 - (msk * maskOpacity);

  view_clipping_distances(world_pos);
}
