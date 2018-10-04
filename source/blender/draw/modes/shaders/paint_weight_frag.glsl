
in vec2 weight_interp; /* (weight, alert) */

out vec4 fragColor;

uniform float opacity = 1.0;
uniform sampler1D colorramp;

void main()
{
	float alert = weight_interp.y;
	vec4 color;

	/* Missing vertex group alert color */
	if (alert > 1.0) {
		color = colorVertexMissingData;
	}
	/* Weights are available */
	else {
		float weight = weight_interp.x;
		vec4 weight_color = texture(colorramp, weight, 0);

		/* Zero weight alert color. Nonlinear blend to reduce impact. */
		color = mix(weight_color, colorVertexUnreferenced, alert * alert);
	}

	/* mix with 1.0 -> is like opacity when using multiply blend mode */
	fragColor = vec4(mix(vec3(1.0), color.rgb, opacity), 1.0);
}
