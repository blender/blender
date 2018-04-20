uniform vec3 color_towards = vec3(0.0, 0.0, 1.0);
uniform vec3 color_outwards = vec3(1.0, 0.0, 0.0);

out vec4 fragColor;

flat in float facing;
void main()
{
	fragColor = vec4((facing < 0.0 ? color_towards: color_outwards)*abs(facing), 1.0);
}
