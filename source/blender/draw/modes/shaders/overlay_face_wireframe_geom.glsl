
/* This shader is only used for edge selection & sculpt mode wires (because of indexed drawing). */

layout(triangles) in;
#ifdef SELECT_EDGES
layout(line_strip, max_vertices = 6) out;
#else
layout(triangle_strip, max_vertices = 3) out;
#endif

in float facing_g[];
in float edgeSharpness_g[];

#ifndef SELECT_EDGES
out float facing;
out vec3 barycentric;
flat out vec3 edgeSharpness;
#endif

void main(void)
{
#ifdef SELECT_EDGES
	const float edge_select_threshold = 0.3;
	if (edgeSharpness_g[0] > edge_select_threshold) {
		gl_Position = gl_in[0].gl_Position; EmitVertex();
		gl_Position = gl_in[1].gl_Position; EmitVertex();
		EndPrimitive();
	}

	if (edgeSharpness_g[1] > edge_select_threshold) {
		gl_Position = gl_in[1].gl_Position; EmitVertex();
		gl_Position = gl_in[2].gl_Position; EmitVertex();
		EndPrimitive();
	}

	if (edgeSharpness_g[2] > edge_select_threshold) {
		gl_Position = gl_in[2].gl_Position; EmitVertex();
		gl_Position = gl_in[0].gl_Position; EmitVertex();
		EndPrimitive();
	}
#else
	edgeSharpness = vec3(edgeSharpness_g[0], edgeSharpness_g[1], edgeSharpness_g[2]);

	barycentric = vec3(1.0, 0.0, 0.0);
	gl_Position = gl_in[0].gl_Position;
	facing = facing_g[0];
	EmitVertex();

	barycentric = vec3(0.0, 1.0, 0.0);
	gl_Position = gl_in[1].gl_Position;
	facing = facing_g[1];
	EmitVertex();

	barycentric = vec3(0.0, 0.0, 1.0);
	gl_Position = gl_in[2].gl_Position;
	facing = facing_g[2];
	EmitVertex();
	EndPrimitive();
#endif /* SELECT_EDGES */
}
