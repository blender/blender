
layout(std140) uniform shadow_render_block {
	mat4 ShadowMatrix[6];
	vec4 lampPosition;
	int layer;
	float exponent;
};

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in vec4 vPos[];

void main() {
	gl_Layer = layer;

	for (int v = 0; v < 3; ++v) {
		gl_Position = vPos[v];
		EmitVertex();
	}

	EndPrimitive();
}