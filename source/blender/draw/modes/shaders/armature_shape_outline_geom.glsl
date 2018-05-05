
/* TODO: See perf with multiple invocations. */
layout(triangles_adjacency) in;
layout(triangle_strip, max_vertices = 16) out;

in vec4 pPos[];
in float vZ[];
in float vFacing[];
in vec2 ssPos[];
in vec2 ssNor[];
in vec4 vColSize[];

flat out vec4 finalColor;
uniform mat4 ProjectionMatrix;
uniform vec2 viewportSize;
uniform float lineThickness = 2.0;

vec2 compute_dir(vec2 v0, vec2 v1)
{
	vec2 dir = normalize(v1 - v0);
	dir = vec2(-dir.y, dir.x);
	return dir;
}

void emit_edge(const ivec3 edges, vec2 thick, bool is_persp)
{
	vec2 edge_dir = compute_dir(ssPos[edges.x], ssPos[edges.y]);
	vec2 hidden_dir = normalize(ssPos[edges.z] - ssPos[edges.x]);

	float fac = dot(-hidden_dir, edge_dir);

	vec2 t = thick * (is_persp ? vZ[edges.x] : 1.0);
	gl_Position = pPos[edges.x];
	EmitVertex();
	gl_Position.xy += t * edge_dir * sign(fac);
	EmitVertex();

	t = thick * (is_persp ? vZ[edges.y] : 1.0);
	gl_Position = pPos[edges.y];
	EmitVertex();
	gl_Position.xy += t * edge_dir * sign(fac);
	EmitVertex();
}

void emit_corner(const int e, vec2 thick, bool is_persp)
{
	vec2 corner_dir = ssNor[e];
	vec2 t = thick * (is_persp ? vZ[e] : 1.0);

	gl_Position = pPos[e] + vec4(t * corner_dir, 0.0, 0.0);
	EmitVertex();
}

void main(void)
{
	finalColor = vec4(vColSize[0].rgb, 1.0);

	vec2 thick = vColSize[0].w * (lineThickness / viewportSize);
	bool is_persp = (ProjectionMatrix[3][3] == 0.0);

	const ivec3 edges = ivec3(0, 2, 4);
	vec4 facing = vec4(vFacing[1], vFacing[3], vFacing[5], vFacing[0]);
	bvec4 do_edge = greaterThanEqual(facing, vec4(0.0));

	/* Only generate outlines from backfaces. */
	if (do_edge.w)
		return;

	if (do_edge.x) {
		emit_corner(edges.x, thick, is_persp);
		emit_edge(edges.xyz, thick, is_persp);
	}

	if (any(do_edge.xy)) {
		emit_corner(edges.y, thick, is_persp);
	}

	if (do_edge.y) {
		emit_edge(edges.yzx, thick, is_persp);
	}
	else {
		EndPrimitive();
	}

	if (any(do_edge.yz)) {
		emit_corner(edges.z, thick, is_persp);
	}

	if (do_edge.z) {
		emit_edge(edges.zxy, thick, is_persp);
		emit_corner(edges.x, thick, is_persp);
	}

	EndPrimitive();
}
