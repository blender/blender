uniform vec3 color_towards = vec3(0.0, 0.0, 1.0);
uniform vec3 color_outwards = vec3(1.0, 0.0, 0.0);

out vec4 fragColor;


void main()
{
	fragColor = vec4(gl_FrontFacing ? color_towards: color_outwards, 0.7);
}
