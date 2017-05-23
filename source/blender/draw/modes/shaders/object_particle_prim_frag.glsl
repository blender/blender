
uniform vec3 color;
uniform sampler1D ramp;

flat in int finalAxis;
flat in float finalVal;

out vec4 fragColor;

void main()
{
	if (finalAxis == -1) {
		if (finalVal < 0.0) {
			fragColor.rgb = color;
		}
		else {
			fragColor.rgb = texture(ramp, finalVal).rgb;
		}
	}
	else {
		fragColor.rgb = vec3(0.0);
		fragColor[finalAxis] = 1.0;
	}

	fragColor.a = 1.0;
}
