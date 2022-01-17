#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform vec4 ClipPlane;

in vec3 pos;

#  if !defined(UNIFORM)
in vec4 color;

out vec4 finalColor_g;
#  endif

#  ifdef CLIP
out float clip_g;
#  endif
#endif

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#if !defined(UNIFORM)
  finalColor_g = color;
#endif

#ifdef CLIP
  clip_g = dot(ModelMatrix * vec4(pos, 1.0), ClipPlane);
#endif
}
