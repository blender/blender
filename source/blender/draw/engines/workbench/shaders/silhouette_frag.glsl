uniform vec3 color;

out vec4 fragColor;

void main()
{
	fragColor = vec4(color, 1.0);
}
