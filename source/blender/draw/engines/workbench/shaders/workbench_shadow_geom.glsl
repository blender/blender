#extension GL_ARB_gpu_shader5 : enable

#ifdef GL_ARB_gpu_shader5
layout(lines_adjacency, invocations = 2) in;
layout(triangle_strip, max_vertices = 4) out;
#else
layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 8) out;
#endif

uniform mat4 ModelMatrixInverse;

uniform vec3 lightDirection = vec3(0.57, 0.57, -0.57);

in VertexData {
	vec3 pos;           /* local position */
	vec4 frontPosition; /* final ndc position */
	vec4 backPosition;
} vData[];

#define DEGENERATE_THRESHOLD 1e-12

#define len_sqr(a) dot(a, a)

void extrude_edge(bool invert)
{
	ivec2 idx = (invert) ? ivec2(2, 1) : ivec2(1, 2);
	gl_Position = vData[idx.x].frontPosition; EmitVertex();
	gl_Position = vData[idx.y].frontPosition; EmitVertex();
	gl_Position = vData[idx.x].backPosition; EmitVertex();
	gl_Position = vData[idx.y].backPosition; EmitVertex();
	EndPrimitive();
}

void main()
{
	/* TODO precompute light_direction */
	vec3 light_dir = mat3(ModelMatrixInverse) * lightDirection;

	vec3 v10 = vData[0].pos - vData[1].pos;
	vec3 v12 = vData[2].pos - vData[1].pos;
	vec3 v13 = vData[3].pos - vData[1].pos;

#ifdef DEGENERATE_THRESHOLD
	vec3 v20 = vData[0].pos - vData[2].pos;
	vec3 v23 = vData[3].pos - vData[2].pos;

	vec4 edges_lensqr = vec4(len_sqr(v10), len_sqr(v13), len_sqr(v20), len_sqr(v23));
	bvec4 degen_edges = lessThan(edges_lensqr, vec4(DEGENERATE_THRESHOLD));

	/* Both triangles are degenerate, abort. */
	if (any(degen_edges.xz) && any(degen_edges.yw))
		return;
#endif

	vec3 n1 = cross(v12, v10);
	vec3 n2 = cross(v13, v12);
	vec2 facing = vec2(dot(n1, light_dir),
	                   dot(n2, light_dir));

	/* WATCH: maybe unpredictable in some cases. */
	bool is_manifold = any(notEqual(vData[0].pos, vData[3].pos));

	bvec2 backface = greaterThan(facing, vec2(0.0));

#ifdef DEGENERATE_THRESHOLD
	/* If one of the 2 triangles is degenerate, replace edge by a non-manifold one. */
	backface.x = (any(degen_edges.xz)) ? !backface.y : backface.x;
	backface.y = (any(degen_edges.yw)) ? !backface.x : backface.y;
	is_manifold = (any(degen_edges)) ? false : is_manifold;
#endif

	/* If both faces face the same direction it's not an outline edge. */
	if (backface.x == backface.y)
		return;

	/* Reverse order if backfacing the light. */
	ivec2 idx = ivec2(1, 2);
	idx = (backface.x) ? idx.yx : idx.xy;

#ifdef GL_ARB_gpu_shader5
	if (gl_InvocationID == 0) {
		extrude_edge(backface.x);
	}
	else if (is_manifold) {
		/* Increment/Decrement twice for manifold edges. */
		extrude_edge(backface.x);
	}
#else
	extrude_edge(backface.x);
	/* Increment/Decrement twice for manifold edges. */
	if (is_manifold) {
		extrude_edge(backface.x);
	}
#endif
}
