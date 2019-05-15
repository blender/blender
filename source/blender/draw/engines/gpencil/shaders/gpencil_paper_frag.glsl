uniform vec3 color;
uniform float opacity;

out vec4 FragColor;

void main()
{
  FragColor = vec4(color, 1.0 - opacity);
}
