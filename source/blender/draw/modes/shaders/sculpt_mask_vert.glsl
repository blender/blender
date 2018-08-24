
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in float msk;

#ifdef SHADE_FLAT
flat out vec4 finalColor;
#else
out vec4 finalColor;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	float mask = 1.0 - msk * 0.75;
	finalColor = vec4(mask, mask, mask, 1.0);
}
