
/* This shader is only used for intel GPU where the Geom shader is faster
 * than doing everything thrice in the vertex shader. */

layout(triangles) in;
#ifdef SELECT_EDGES
layout(line_strip, max_vertices = 6) out;
#else
layout(triangle_strip, max_vertices = 3) out;
#endif

uniform vec2 wireStepParam;

in vec2 ssPos[];
in float facingOut[];

#ifndef SELECT_EDGES
out vec3 barycentric;
out float facing;
#endif

#ifdef LIGHT_EDGES
in vec3 obPos[];
in vec3 vNor[];
in float forceEdge[];

#  ifndef SELECT_EDGES
flat out vec3 edgeSharpness;
#  endif
#endif

#define NO_EDGE vec3(10000.0);

vec3 get_edge_normal(vec3 n1, vec3 n2, vec3 edge)
{
	edge = normalize(edge);
	vec3 n = n1 + n2;
	float p = dot(edge, n);
	return normalize(n - p * edge);
}

float get_edge_sharpness(vec3 fnor, vec3 vnor)
{
	float sharpness = abs(dot(fnor, vnor));
	return smoothstep(wireStepParam.x, wireStepParam.y, sharpness);
}

vec3 get_barycentric(bvec3 do_edge, const int v)
{
	int v_n = v;
	int v_n1 = (v + 1) % 3;
	int v_n2 = (v + 2) % 3;
	vec3 bary;
	bary[v_n] = do_edge[v_n] ? 0.0 : 1.0;
	bary[v_n1] = 1.0;
	bary[v_n2] = do_edge[v_n2] ? 0.0 : 1.0;
	return bary;
}

void main(void)
{
	vec3 facings = vec3(facingOut[0], facingOut[1], facingOut[2]);
	bvec3 do_edge = greaterThan(abs(facings), vec3(1.0));
	facings = fract(facings) - clamp(-sign(facings), 0.0, 1.0);

#ifdef SELECT_EDGES
	vec3 edgeSharpness;
#endif

#ifdef LIGHT_EDGES
	vec3 edges[3];
	edges[0] = obPos[1] - obPos[0];
	edges[1] = obPos[2] - obPos[1];
	edges[2] = obPos[0] - obPos[2];
	vec3 fnor = normalize(cross(edges[0], -edges[2]));

	edgeSharpness.x = get_edge_sharpness(fnor, get_edge_normal(vNor[0], vNor[1], edges[0]));
	edgeSharpness.y = get_edge_sharpness(fnor, get_edge_normal(vNor[1], vNor[2], edges[1]));
	edgeSharpness.z = get_edge_sharpness(fnor, get_edge_normal(vNor[2], vNor[0], edges[2]));
	edgeSharpness.x = (forceEdge[0] == 1.0) ? 1.0 : edgeSharpness.x;
	edgeSharpness.y = (forceEdge[1] == 1.0) ? 1.0 : edgeSharpness.y;
	edgeSharpness.z = (forceEdge[2] == 1.0) ? 1.0 : edgeSharpness.z;
#endif

#ifdef SELECT_EDGES
	const float edge_select_threshold = 0.3;
	if (edgeSharpness.x > edge_select_threshold) {
		gl_Position = gl_in[0].gl_Position;
		EmitVertex();
		gl_Position = gl_in[1].gl_Position;
		EmitVertex();
		EndPrimitive();
	}

	if (edgeSharpness.y > edge_select_threshold) {
		gl_Position = gl_in[1].gl_Position;
		EmitVertex();
		gl_Position = gl_in[2].gl_Position;
		EmitVertex();
		EndPrimitive();
	}

	if (edgeSharpness.z > edge_select_threshold) {
		gl_Position = gl_in[2].gl_Position;
		EmitVertex();
		gl_Position = gl_in[0].gl_Position;
		EmitVertex();
		EndPrimitive();
	}
#else
	barycentric = get_barycentric(do_edge, 0);
	gl_Position = gl_in[0].gl_Position;
	facing = facings.x;
	EmitVertex();

	barycentric = get_barycentric(do_edge, 1);
	gl_Position = gl_in[1].gl_Position;
	facing = facings.y;
	EmitVertex();

	barycentric = get_barycentric(do_edge, 2);
	gl_Position = gl_in[2].gl_Position;
	facing = facings.z;
	EmitVertex();
	EndPrimitive();
#endif
}
