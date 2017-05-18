
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
#if defined(USE_COLOR_U32)
in uint color;
#else
in vec4 color;
#endif

flat out vec4 finalColor;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

#if defined(USE_COLOR_U32)
	finalColor = vec4(
		((color      ) & uint(0xFF)) * (1.0f / 255.0f),
		((color >>  8) & uint(0xFF)) * (1.0f / 255.0f),
		((color >> 16) & uint(0xFF)) * (1.0f / 255.0f),
		((color >> 24)             ) * (1.0f / 255.0f));
#else
	finalColor = color;
#endif
}
