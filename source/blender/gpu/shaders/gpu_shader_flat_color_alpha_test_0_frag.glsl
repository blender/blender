
#if __VERSION__ == 120
  flat varying vec4 finalColor;
  #define fragColor gl_FragColor
#else
  flat in vec4 finalColor;
  out vec4 fragColor;
#endif

void main()
{
	if (finalColor.a > 0.0)
		fragColor = finalColor;
	else
		discard;
}
