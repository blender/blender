uniform vec3 color;
uniform float opacity;

out vec4 FragColor;

void main()
{
	FragColor = vec4(color, opacity);
}
