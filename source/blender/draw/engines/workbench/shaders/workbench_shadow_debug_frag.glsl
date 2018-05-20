
out vec4 fragColor;

void main()
{
	const float intensity = 0.25;
#ifdef SHADOW_PASS
	fragColor = vec4((gl_FrontFacing) ? vec3(intensity, -intensity, 0.0)
	                                  : vec3(-intensity, intensity, 0.0), 1.0);
#else
	fragColor = vec4((gl_FrontFacing) ? vec3(intensity, intensity, -intensity)
	                                  : vec3(-intensity, -intensity, intensity), 1.0);
#endif
}
