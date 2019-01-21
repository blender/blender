
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

void vert_from_gl_in(int v)
{
	gl_Position = gl_in[v].gl_Position;
#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_set_clip_distance(gl_in[v].gl_ClipDistance);
#endif
}

void main(void)
{
#ifdef SELECT_EDGES
	const float edge_select_threshold = 0.3;
	if (edgeSharpness_g[0] > edge_select_threshold) {
		vert_from_gl_in(0);
		EmitVertex();
		vert_from_gl_in(1);
		EmitVertex();

		EndPrimitive();
	}

	if (edgeSharpness_g[1] > edge_select_threshold) {
		vert_from_gl_in(1);
		EmitVertex();
		vert_from_gl_in(2);
		EmitVertex();

		EndPrimitive();
	}

	if (edgeSharpness_g[2] > edge_select_threshold) {
		vert_from_gl_in(2);
		EmitVertex();
		vert_from_gl_in(0);
		EmitVertex();

		EndPrimitive();
	}
#else
	/* Originally was:
	 *   edgeSharpness = vec3(edgeSharpness_g[0], edgeSharpness_g[1], edgeSharpness_g[2]);
	 *
	 * But that strangely does not work for some AMD GPUs.
	 * However since this code is currently only used for sculpt mode
	 * and in this mode the `edgeSharpness_g` is not calculated,
	 * let's simply set all to 1.0.
	 */
	edgeSharpness = vec3(1.0);

	barycentric = vec3(1.0, 0.0, 0.0);
	vert_from_gl_in(0);
	facing = facing_g[0];
	EmitVertex();

	barycentric = vec3(0.0, 1.0, 0.0);
	vert_from_gl_in(1);
	facing = facing_g[1];
	EmitVertex();

	barycentric = vec3(0.0, 0.0, 1.0);
	vert_from_gl_in(2);
	facing = facing_g[2];
	EmitVertex();
	EndPrimitive();
#endif /* SELECT_EDGES */
}
