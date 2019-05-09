
uniform mat4 ModelMatrix;

in vec3 pos;

out vec4 pPos;
out vec3 vPos;

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  vPos = point_world_to_view(world_pos);
  pPos = point_world_to_ndc(world_pos);
  /* Small bias to always be on top of the geom. */
  pPos.z -= 1e-3;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos).xyz);
#endif
}
