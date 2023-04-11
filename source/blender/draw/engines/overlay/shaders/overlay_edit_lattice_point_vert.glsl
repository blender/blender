
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  if ((data & VERT_SELECTED) != 0u) {
    finalColor = colorVertexSelect;
  }
  else if ((data & VERT_ACTIVE) != 0u) {
    finalColor = colorEditMeshActive;
  }
  else {
    finalColor = colorVertex;
  }

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  /* Small offset in Z */
  gl_Position.z -= 3e-4;

  gl_PointSize = sizeVertex * 2.0;

  view_clipping_distances(world_pos);
}
