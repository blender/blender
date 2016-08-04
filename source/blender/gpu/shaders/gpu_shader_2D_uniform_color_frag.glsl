
uniform vec4 color;

#if __VERSION__ == 120
  #define fragColor gl_FragColor
#else
  out vec4 fragColor;
#endif

void main()
{
	fragColor = color;
}
