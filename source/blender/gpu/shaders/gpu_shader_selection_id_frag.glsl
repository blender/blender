
#ifdef UNIFORM_ID
uniform uint id;
#else
flat in vec4 id;
#endif

out vec4 fragColor;

void main()
{
#ifdef UNIFORM_ID
	fragColor = vec4(
		((id      ) & uint(0xFF)) * (1.0f / 255.0f),
		((id >>  8) & uint(0xFF)) * (1.0f / 255.0f),
		((id >> 16) & uint(0xFF)) * (1.0f / 255.0f),
		((id >> 24)             ) * (1.0f / 255.0f));
#else
	fragColor = id;
#endif
}
