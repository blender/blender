
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
#if defined(USE_COLOR_U32)
in int color;
#else
in vec4 color;
#endif

flat out vec4 finalColor;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

#if defined(USE_COLOR_U32)
	finalColor = vec4(
		((color      ) & 0xFF) * (1.0f / 255.0f),
		((color >>  8) & 0xFF) * (1.0f / 255.0f),
		((color >> 16) & 0xFF) * (1.0f / 255.0f),
		((color >> 24)       ) * (1.0f / 255.0f));
#else
	finalColor = color;
#endif
}
