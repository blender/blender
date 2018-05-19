layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 8) out;

uniform mat4 ModelMatrixInverse;

uniform vec3 lightDirection = vec3(0.57, 0.57, -0.57);

in VertexData {
	vec3 pos;           /* local position */
	vec4 frontPosition; /* final ndc position */
	vec4 backPosition;
} vData[];

void main()
{
	/* TODO precompute light_direction */
	vec3 light_dir = mat3(ModelMatrixInverse) * lightDirection;

	vec3 v10 = vData[0].pos - vData[1].pos;
	vec3 v12 = vData[2].pos - vData[1].pos;
	vec3 v13 = vData[3].pos - vData[1].pos;
	vec3 n1 = cross(v12, v10);
	vec3 n2 = cross(v13, v12);
	vec2 facing = vec2(dot(n1, light_dir),
	                   dot(n2, light_dir));
	bvec2 backface = greaterThan(facing, vec2(0.0));

	if (backface.x == backface.y) {
		/* Both faces face the same direction. Not an outline edge. */
		return;
	}

	/* Reverse order if backfacing the light. */
	ivec2 idx = ivec2(1, 2);
	idx = (backface.x) ? idx.yx : idx.xy;

	/* WATCH: maybe unpredictable in some cases. */
	bool is_manifold = any(notEqual(vData[0].pos, vData[3].pos));

	gl_Position = vData[idx.x].frontPosition; EmitVertex();
	gl_Position = vData[idx.y].frontPosition; EmitVertex();
	gl_Position = vData[idx.x].backPosition; EmitVertex();
	gl_Position = vData[idx.y].backPosition; EmitVertex();
	EndPrimitive();

	if (is_manifold) {
		gl_Position = vData[idx.x].frontPosition; EmitVertex();
		gl_Position = vData[idx.y].frontPosition; EmitVertex();
		gl_Position = vData[idx.x].backPosition; EmitVertex();
		gl_Position = vData[idx.y].backPosition; EmitVertex();
		EndPrimitive();
	}
}
