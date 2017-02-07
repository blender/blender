
uniform vec4 color;
uniform vec3 light;

#if __VERSION__ == 120
  varying vec3 normal;
  #define fragColor gl_FragColor
#else
  in vec3 normal;
  out vec4 fragColor;
#endif

void main()
{
	fragColor = color * max(0.0, dot(normalize(normal), light));
}
