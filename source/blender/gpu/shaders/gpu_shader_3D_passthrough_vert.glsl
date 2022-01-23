#ifndef USE_GPU_SHADER_CREATE_INFO
#  ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#  endif

/* Does Nothing */
in vec3 pos;
#endif

void main()
{
  vec4 pos_4d = vec4(pos, 1.0);
  gl_Position = pos_4d;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * pos_4d).xyz);
#endif
}
