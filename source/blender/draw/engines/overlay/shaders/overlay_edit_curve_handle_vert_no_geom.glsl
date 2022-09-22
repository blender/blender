/* TODO(Metal): Implement correct SSBO implementation for geom shader workaround.
 * Currently included as placeholder to unblock failing compilation in Metal. */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);
  vert.flag = data;

  view_clipping_distances(world_pos);
}