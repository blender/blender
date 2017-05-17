
uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;

in vec3 pos;
in vec3 nor;
in vec4 color;

#ifdef USE_FLAT_NORMAL
flat out vec3 normal;
flat out vec4 finalColor;
#else
out vec3 normal;
out vec4 finalColor;
#endif

void main()
{
	normal = normalize(NormalMatrix * nor);
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	finalColor = color;
}
