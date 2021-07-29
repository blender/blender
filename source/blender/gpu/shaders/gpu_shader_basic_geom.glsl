/*
 * Used the implementation of wide lines of Timo Suoranta (http://neure.dy.fi/wideline.html)
 */

#define PASSTHROUGH             0

layout(lines) in;

#if defined(DRAW_LINE)

#if PASSTHROUGH
layout(line_strip, max_vertices = 10) out;
#else
layout(triangle_strip, max_vertices = 6) out;
#endif

varying out float t;
varying in vec4 varying_vertex_color_line[];
varying out vec4 varying_vertex_color;

uniform ivec4 viewport;
uniform float line_width;
uniform int stipple_factor;

void main(void)
{
	vec2 window_size = viewport.zw;
	vec4 start  = gl_in[0].gl_Position;
	vec4 end    = gl_in[1].gl_Position;
#if PASSTHROUGH
	gl_Position = start; EmitVertex();
	gl_Position = end; EmitVertex();
	EndPrimitive();
	return;
#endif

	/* t = 0                                 t = ~(len(end - start) + 2*line_width)
	 * A-------------------------------------B
	 * |        |                   |        |
	 * |       side                 |        |
	 * |        |                   |        |
	 * |--axis--*start--------------*end-----|
	 * |        |                   |        |
	 * |        |                   |        |
	 * |        |                   |        |
	 * D-------------------------------------C
	 */

	/* Clip the line before homogenization.
	 * Compute line start and end distances to nearplane in clipspace
	 * Distances are t0 = dot(start, plane) and t1 = dot(end, plane)
	 */
	float t0 = start.z + start.w;
	float t1 = end.z + end.w;
	if (t0 < 0.0) {
		if (t1 < 0.0) {
			return;
		}
		start = mix(start, end, (0 - t0) / (t1 - t0));
	}
	if (t1 < 0.0) {
		end = mix(start, end, (0 - t0) / (t1 - t0));
	}

	/* Compute line axis and side vector in screen space */
	vec2 startInNDC     = start.xy / start.w;                      /* clip to NDC: homogenize and drop z */
	vec2 endInNDC       = end.xy / end.w;
	vec2 lineInNDC      = endInNDC - startInNDC;
	vec2 lineInScreen   = lineInNDC * window_size;                 /* ndc to screen (direction vector) */

	vec2 axisInScreen   = normalize(lineInScreen);
	vec2 sideInScreen   = vec2(-axisInScreen.y, axisInScreen.x);   /* rotate */
	vec2 axisInNDC      = axisInScreen / window_size;              /* screen to NDC */
	vec2 sideInNDC      = sideInScreen / window_size;
	vec4 axis           = vec4(axisInNDC, 0.0, 0.0) * line_width;  /* NDC to clip (delta vector) */
	vec4 side           = vec4(sideInNDC, 0.0, 0.0) * line_width;

	vec4 A = (start + (side - axis) * start.w);
	vec4 B = (end   + (side + axis) * end.w);
	vec4 C = (end   - (side - axis) * end.w);
	vec4 D = (start - (side + axis) * start.w);

	/* There is no relation between lines yet */
	/* TODO Pass here t0 to make continuous pattern. */
	t0 = 0;
	t1 = (length(lineInScreen) + 2 * line_width) / (2 * line_width * stipple_factor);

	gl_Position = A; t = t0; varying_vertex_color = varying_vertex_color_line[0]; EmitVertex();
	gl_Position = D; t = t0; varying_vertex_color = varying_vertex_color_line[0]; EmitVertex();
	gl_Position = B; t = t1; varying_vertex_color = varying_vertex_color_line[1]; EmitVertex();
	gl_Position = C; t = t1; varying_vertex_color = varying_vertex_color_line[1]; EmitVertex();
	EndPrimitive();
}

#else
void main(void)
{

}
#endif
