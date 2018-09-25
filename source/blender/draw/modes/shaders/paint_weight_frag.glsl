
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

	/* See gpu_shader_multiply_and_blend_preprocessing.glsl */
	fragColor = vec4(color.rgb * opacity + (1 - opacity), 1.0);
}
