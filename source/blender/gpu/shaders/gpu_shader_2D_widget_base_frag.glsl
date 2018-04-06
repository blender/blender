uniform vec3 checkerColorAndSize;

noperspective in vec4 finalColor;
noperspective in float butCo;

out vec4 fragColor;

vec4 do_checkerboard()
{
	float size = checkerColorAndSize.z;
	vec2 phase = mod(gl_FragCoord.xy, size * 2.0);

	if ((phase.x > size && phase.y < size) ||
		(phase.x < size && phase.y > size))
	{
		return vec4(checkerColorAndSize.xxx, 1.0);
	}
	else {
		return vec4(checkerColorAndSize.yyy, 1.0);
	}
}

void main()
{
	fragColor = finalColor;

	if (butCo > 0.5) {
		fragColor = mix(do_checkerboard(), fragColor, fragColor.a);
	}

	if (butCo > 0.0) {
		fragColor.a = 1.0;
	}
}