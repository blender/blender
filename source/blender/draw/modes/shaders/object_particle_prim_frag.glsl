
uniform vec4 color;

flat in int finalAxis;

out vec4 fragColor;

void main()
{
	if (finalAxis == -1) {
		fragColor = color;
	}
	else {
		vec4 col = vec4(0.0);
		col[finalAxis] = 1.0;
		col.a = 1.0;
		fragColor = col;
	}
}
