
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

layout(std140) uniform shadow_render_block {
	mat4 ShadowMatrix[6];
	int Layer;
};

in vec4 vPos[];
in int face[];

void main() {
	int f = face[0];
	gl_Layer = Layer + f;

	for (int v = 0; v < 3; ++v) {
		gl_Position = ShadowMatrix[f] * vPos[v];
		EmitVertex();
	}

	EndPrimitive();
}