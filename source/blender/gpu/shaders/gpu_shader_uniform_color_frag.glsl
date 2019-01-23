
#if defined(USE_COLOR_U32)
uniform uint color;
#else
uniform vec4 color;
#endif

out vec4 fragColor;

void main()
{
#if defined(USE_COLOR_U32)
	fragColor = vec4(
		((color      ) & uint(0xFF)) * (1.0f / 255.0f),
		((color >>  8) & uint(0xFF)) * (1.0f / 255.0f),
		((color >> 16) & uint(0xFF)) * (1.0f / 255.0f),
		((color >> 24)             ) * (1.0f / 255.0f));
#else
	fragColor = color;
#endif

#if defined(USE_BACKGROUND)
	gl_FragDepth = 1.0;
#endif
}
