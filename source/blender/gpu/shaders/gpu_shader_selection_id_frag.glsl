
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
		((id       ) & 0xFFu) * (1.0f / 255.0f),
		((id >>  8u) & 0xFFu) * (1.0f / 255.0f),
		((id >> 16u) & 0xFFu) * (1.0f / 255.0f),
		((id >> 24u)        ) * (1.0f / 255.0f));
#else
	fragColor = id;
#endif
}
