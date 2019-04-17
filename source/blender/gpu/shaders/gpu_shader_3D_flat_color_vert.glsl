
uniform mat4 ModelViewProjectionMatrix;
#ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#endif

in vec3 pos;
#if defined(USE_COLOR_U32)
in uint color;
#else
in vec4 color;
#endif

flat out vec4 finalColor;

void main()
{
  vec4 pos_4d = vec4(pos, 1.0);
  gl_Position = ModelViewProjectionMatrix * pos_4d;

#if defined(USE_COLOR_U32)
  finalColor = vec4(((color)&uint(0xFF)) * (1.0f / 255.0f),
                    ((color >> 8) & uint(0xFF)) * (1.0f / 255.0f),
                    ((color >> 16) & uint(0xFF)) * (1.0f / 255.0f),
                    ((color >> 24)) * (1.0f / 255.0f));
#else
  finalColor = color;
#endif

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * pos_4d).xyz);
#endif
}
