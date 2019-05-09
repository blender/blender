
uniform mat4 ModelMatrix;

in vec2 u; /* active uv map */
in vec3 pos;

#ifdef TEXTURE_PAINT_MASK
in vec2 mu; /* masking uv map */
#endif

out vec2 uv_interp;

#ifdef TEXTURE_PAINT_MASK
out vec2 masking_uv_interp;
#endif

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  uv_interp = u;

#ifdef TEXTURE_PAINT_MASK
  masking_uv_interp = mu;
#endif

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
