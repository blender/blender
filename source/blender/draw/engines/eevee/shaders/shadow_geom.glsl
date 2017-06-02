
layout(std140) uniform shadow_render_block {
	mat4 ShadowMatrix[6];
	vec4 lampPosition;
	int layer;
	float exponent;
};

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in vec4 vPos[];
flat in int face[];

out vec3 worldPosition;

void main() {
	int f = face[0];
	gl_Layer = f;

	for (int v = 0; v < 3; ++v) {
		gl_Position = ShadowMatrix[f] * vPos[v];
		worldPosition = vPos[v].xyz;
		EmitVertex();
	}

	EndPrimitive();
}