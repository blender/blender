#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ViewProjectionMatrix;

/* ---- Instantiated Attrs ---- */
in vec3 pos;

/* ---- Per instance Attrs ---- */
in mat4 InstanceModelMatrix;
in vec4 color;
#  ifdef UNIFORM_SCALE
in float size;
#  else
in vec3 size;
#  endif

flat out vec4 finalColor;
#endif

void main()
{
  finalColor = color;

  vec4 wPos = InstanceModelMatrix * vec4(pos * size, 1.0);
  gl_Position = ViewProjectionMatrix * wPos;

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(wPos.xyz);
#endif
}
