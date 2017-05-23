
uniform vec3 color;

flat in int finalAxis;

out vec4 fragColor;

void main()
{
	if (finalAxis == -1) {
		fragColor.rgb = color;
	}
	else {
		fragColor.rgb = vec3(0.0);
		fragColor[finalAxis] = 1.0;
	}

	fragColor.a = 1.0;
}
