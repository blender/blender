layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

uniform float outlineWidth = 1.0;
uniform vec2 viewportSize;

in vec4 pos_xformed[];
in float widthModulator[];

noperspective out vec3 distanceToOutline;

// project to screen space
vec2 proj(int axis) {
	vec4 pos = pos_xformed[axis];
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

float dist(vec2 pos[3], int v) {
	// current vertex position
	vec2 vpos = pos[v];
	// endpoints of opposite edge
	vec2 e1 = pos[(v + 1) % 3];
	vec2 e2 = pos[(v + 2) % 3];

	float abs_det = length(cross(vec3(vpos - e1, 0), vec3(vpos - e2, 0))); // could simplify
	return abs_det / distance(e2, e1);
}

vec3 distance[3];

void clearEdge(int v) {
	float distant = 10 * outlineWidth;
	for (int i = 0; i < 3; ++i)
		distance[i][v] += distant;
}

void modulateEdge(int v) {
	float offset = min(widthModulator[v],1) * outlineWidth;
	for (int i = 0; i < 3; ++i)
		distance[i][v] -= offset;
}

void main() {
	vec2 pos[3] = vec2[3](proj(0), proj(1), proj(2));

	for (int v = 0; v < 3; ++v)
		distance[v] = vec3(0);

	for (int v = 0; v < 3; ++v) {
		if (widthModulator[v] > 0) {
			distance[v][v] = dist(pos, v);
			modulateEdge(v);
		}
	}

	for (int v = 0; v < 3; ++v)
		if (widthModulator[v] <= 0)
			clearEdge(v);

	for (int v = 0; v < 3; ++v) {
		gl_Position = pos_xformed[v];
		distanceToOutline = distance[v];
		EmitVertex();
	}

	EndPrimitive();
}
