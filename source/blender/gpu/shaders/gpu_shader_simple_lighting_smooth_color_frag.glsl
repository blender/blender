
uniform vec3 light;

#ifdef USE_FLAT_NORMAL
flat in vec3 normal;
flat in vec4 finalColor;
#else
in vec3 normal;
in vec4 finalColor;
#endif
out vec4 fragColor;

void main()
{
	fragColor = finalColor * max(0.0, dot(normalize(normal), light));
}
