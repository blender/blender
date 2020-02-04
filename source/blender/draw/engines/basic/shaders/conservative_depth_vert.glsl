
RESOURCE_ID_VARYING

in vec3 pos;

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND
  PASS_RESOURCE_ID

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
