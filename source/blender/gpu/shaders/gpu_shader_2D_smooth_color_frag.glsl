
#if __VERSION__ == 120
  noperspective varying vec4 finalColor;
  #define fragColor gl_FragColor
#else
  noperspective in vec4 finalColor;
  out vec4 fragColor;
#endif

void main()
{
	fragColor = finalColor;
}
