
layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 6) out;

in vec4 pPos[];
in vec3 vPos[];
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

void emit_edge(vec2 edge_dir, vec2 hidden_dir, vec2 thick, bool is_persp)
{
	float fac = dot(-hidden_dir, edge_dir);
	edge_dir *= (fac < 0.0) ? -1.0 : 1.0;

	vec2 t = thick * (is_persp ? abs(vPos[1].z) : 1.0);
	gl_Position = pPos[1];
	EmitVertex();
	gl_Position.xy += t * edge_dir;
	EmitVertex();

	t = thick * (is_persp ? abs(vPos[2].z) : 1.0);
	gl_Position = pPos[2];
	EmitVertex();
	gl_Position.xy += t * edge_dir;
	EmitVertex();
}

void emit_corner(const int e, vec2 thick, bool is_persp)
{
	vec2 corner_dir = ssNor[e];
	vec2 t = thick * (is_persp ? abs(vPos[e].z) : 1.0);

	gl_Position = pPos[e] + vec4(t * corner_dir, 0.0, 0.0);
	EmitVertex();
}

void main(void)
{
	finalColor = vec4(vColSize[0].rgb, 1.0);

	bool is_persp = (ProjectionMatrix[3][3] == 0.0);

	vec3 view_vec = (is_persp) ? normalize(vPos[1]) : vec3(0.0, 0.0, -1.0);
	vec3 v10 = vPos[0] - vPos[1];
	vec3 v12 = vPos[2] - vPos[1];
	vec3 v13 = vPos[3] - vPos[1];

	vec3 n0 = cross(v12, v10);
	vec3 n3 = cross(v13, v12);

	float fac0 = dot(view_vec, n0);
	float fac3 = dot(view_vec, n3);

	/* If both adjacent verts are facing the camera the same way,
	 * then it isn't an outline edge. */
	if (sign(fac0) == sign(fac3))
		return;

	/* Don't outline if concave edge. */
	if (dot(n0, v13) > 0.0001)
		return;

	vec2 thick = vColSize[0].w * (lineThickness / viewportSize);
	vec2 edge_dir = compute_dir(ssPos[1], ssPos[2]);

	vec2 hidden_point;
	/* Take the farthest point to compute edge direction
	 * (avoid problems with point behind near plane).
	 * If the chosen point is parallel to the edge in screen space,
	 * choose the other point anyway.
	 * This fixes some issue with cubes in orthographic views.*/
	if (vPos[0].z < vPos[3].z) {
		hidden_point = (abs(fac0) > 1e-5) ? ssPos[0] : ssPos[3];
	}
	else {
		hidden_point = (abs(fac3) > 1e-5) ? ssPos[3] : ssPos[0];
	}
	vec2 hidden_dir = normalize(hidden_point - ssPos[1]);

	emit_corner(1, thick, is_persp);
	emit_edge(edge_dir, hidden_dir, thick, is_persp);
	emit_corner(2, thick, is_persp);
	EndPrimitive();
}
