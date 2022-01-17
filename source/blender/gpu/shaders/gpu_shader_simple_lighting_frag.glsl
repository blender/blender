#ifndef USE_GPU_SHADER_CREATE_INFO
#  ifndef USE_INSTANCE_COLOR
uniform vec4 color;
#  endif
uniform vec3 light;

in vec3 normal;
#  ifdef USE_INSTANCE_COLOR
flat in vec4 finalColor;
#    define color finalColor
#  endif
out vec4 fragColor;
#endif

void main()
{
  fragColor = simple_lighting_data.color;
  fragColor.xyz *= clamp(dot(normalize(normal), simple_lighting_data.light), 0.0, 1.0);
}
