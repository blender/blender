
uniform vec3 light;

#if __VERSION__ == 120
#  ifdef USE_FLAT_NORMAL
  flat varying vec3 normal;
  flat varying vec4 finalColor;
#  else
  varying vec3 normal;
  varying vec4 finalColor;
#  endif
  #define fragColor gl_FragColor
#else
#  ifdef USE_FLAT_NORMAL
  flat in vec3 normal;
  flat in vec4 finalColor;
#  else
  in vec3 normal;
  in vec4 finalColor;
#  endif
  out vec4 fragColor;
#endif

void main()
{
	fragColor = finalColor * max(0.0, dot(normalize(normal), light));
}
