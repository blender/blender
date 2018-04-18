uniform vec3 color;

in vec3 normal;
out vec4 fragColor;

void main()
{
	float intensity = dot(normal, vec3(0.0, 0.0, 1.0));
	vec3 shaded_color = color * intensity;
	fragColor = vec4(shaded_color, 1.0);
}
