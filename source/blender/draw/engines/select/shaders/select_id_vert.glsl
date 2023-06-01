#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
#ifndef UNIFORM_ID
  select_id = offset + index;
#endif

  vec3 world_pos = point_object_to_world(pos);
  vec3 view_pos = point_world_to_view(world_pos);
  gl_Position = point_view_to_ndc(view_pos);
  gl_PointSize = sizeVertex;

  /* Offset Z position for retopology selection occlusion. */
  gl_Position.z += get_homogenous_z_offset(view_pos.z, gl_Position.w, retopologyOffset);

  view_clipping_distances(world_pos);
}
