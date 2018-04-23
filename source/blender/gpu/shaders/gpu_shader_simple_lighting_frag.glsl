
#ifndef USE_INSTANCE_COLOR
uniform vec4 color;
#endif
uniform vec3 light;

in vec3 normal;
#ifdef USE_INSTANCE_COLOR
flat in vec4 finalColor;
#  define color finalColor
#endif
out vec4 fragColor;

void main()
{
	fragColor = color;
	fragColor.xyz *= clamp(dot(normalize(normal), light), 0.0, 1.0);
}
