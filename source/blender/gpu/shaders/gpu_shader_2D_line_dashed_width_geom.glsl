
// Draw dashed lines, perforated in screen space, with non-unary width.

/* Make to be used with dynamic batching so no Model Matrix needed */
uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewport_size;

/* Width of the generated 'line'. */
uniform float width;  /* in pixels, screen space. */

/* Uniforms from fragment shader, used here to optimize out useless computation in case of solid line. */
uniform float dash_factor;  /* if > 1.0, solid line. */
uniform int colors_len;  /* Enabled if > 0, 1 for solid line. */

layout(lines) in;

layout(triangle_strip, max_vertices = 4) out;
noperspective out float distance_along_line;

void main()
{
	vec4 v1 = gl_in[0].gl_Position;
	vec4 v2 = gl_in[1].gl_Position;

	/* Width, from 2D screen space in pixels, to ModelViewProjection space of each input vertices. */
	float w1 = (width / viewport_size) * v1.w * 2.0;
	float w2 = (width / viewport_size) * v2.w * 2.0;

	/* Normalized vector parallel to screen and orthogonal to line. */
	vec4 wdir = normalize(vec4(v1.y - v2.y, v2.x - v1.x, 0.0, 0.0))

	distance_along_line = 0.0f;
	gl_Position = v1 + (wdir * w1);
	EmitVertex();

	gl_Position = v1 - (wdir * w1);
	EmitVertex();

	if ((colors_len == 1) || (dash_factor >= 1.0f)) {
		/* Solid line, optimize out distance computation! */
		distance_along_line = 0.0f;
	}
	else {
		vec2 p1 = (v1.xy / v1.w) * 0.5 + 0.5;  // <- device coordinates in [0..1] range.
		p1 = p1 * viewport_size;  // <- 'virtual' screen coordinates.

		vec2 p2 = (v2.xy / v2.w) * 0.5 + 0.5;  // <- device coordinates in [0..1] range.
		p2 = p2 * viewport_size;  // <- 'virtual' screen coordinates.

		distance_along_line = distance(p1, p2);
	}
	gl_Position = v2 + (wdir * w2);
	EmitVertex();

	gl_Position = v2 - (wdir * w2);
	EmitVertex();

	EndPrimitive();
}
