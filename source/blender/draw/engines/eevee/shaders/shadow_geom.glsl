
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

uniform mat4 ShadowMatrix;
uniform int Layer;

in vec4 vPos[];

void main() {
	gl_Layer = Layer;
	gl_Position = ShadowMatrix * vPos[0];
	EmitVertex();
	gl_Layer = Layer;
	gl_Position = ShadowMatrix * vPos[1];
	EmitVertex();
	gl_Layer = Layer;
	gl_Position = ShadowMatrix * vPos[2];
	EmitVertex();
	EndPrimitive();
}