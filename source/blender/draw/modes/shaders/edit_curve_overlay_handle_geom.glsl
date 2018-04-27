
#define ACTIVE_NURB    1 << 7 /* Keep the same value of `ACTIVE_NURB` in `draw_cache_imp_curve.c` */

layout(lines) in;
layout(line_strip, max_vertices = 6) out;

uniform vec2 viewportSize;

flat in int vertFlag[];

flat out vec4 finalColor;

void main()
{
	/* TODO: vertex size */

	vec4 v1 = gl_in[0].gl_Position;
	vec4 v2 = gl_in[1].gl_Position;

	int is_active_nurb = vertFlag[1] & ACTIVE_NURB;
	int color_id = vertFlag[1] ^ is_active_nurb;

	if (is_active_nurb != 0) {
		/* draw the outline. */
		vec2 v1_2 = (v2.xy/v2.w - v1.xy/v1.w);
		vec2 offset;

		if (abs(v1_2.x * viewportSize.x) < abs(v1_2.y * viewportSize.y)) {
			offset = vec2(2.0 / viewportSize.x, 0.0);
		}
		else {
			offset = vec2(0.0, 2.0 / viewportSize.y);
		}

		finalColor = colorActiveSpline;

		gl_Position = v1;
		gl_Position.xy += offset * v1.w;
		EmitVertex();

		gl_Position = v2;
		gl_Position.xy += offset * v2.w;
		EmitVertex();

		EndPrimitive();

		gl_Position = v1;
		gl_Position.xy -= offset * v1.w;
		EmitVertex();

		gl_Position = v2;
		gl_Position.xy -= offset * v2.w;
		EmitVertex();

		EndPrimitive();
	}

	if      (color_id == 0)  finalColor = colorHandleFree;
	else if (color_id == 1)  finalColor = colorHandleAuto;
	else if (color_id == 2)  finalColor = colorHandleVect;
	else if (color_id == 3)  finalColor = colorHandleAlign;
	else if (color_id == 4)  finalColor = colorHandleAutoclamp;
	else if (color_id == 5)  finalColor = colorHandleSelFree;
	else if (color_id == 6)  finalColor = colorHandleSelAuto;
	else if (color_id == 7)  finalColor = colorHandleSelVect;
	else if (color_id == 8)  finalColor = colorHandleSelAlign;
	else if (color_id == 9)  finalColor = colorHandleSelAutoclamp;
	else if (color_id == 10) finalColor = colorNurbUline;
	else if (color_id == 11) finalColor = colorNurbSelUline;
	else                     finalColor = colorVertexSelect;

	gl_Position = v1;
	EmitVertex();

	gl_Position = v2;
	EmitVertex();

	EndPrimitive();
}
