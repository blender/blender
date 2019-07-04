
in vec3 pos;
in vec2 mu; /* masking uv map */

out vec2 masking_uv_interp;

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  masking_uv_interp = mu;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
