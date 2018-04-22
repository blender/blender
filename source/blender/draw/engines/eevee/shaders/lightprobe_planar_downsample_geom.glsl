
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in int instance[];
in vec2 vPos[];

flat out float layer;

void main() {
	gl_Layer = instance[0];
	layer = float(instance[0]);

	gl_Position = vec4(vPos[0], 0.0, 1.0);
	EmitVertex();

	gl_Position = vec4(vPos[1], 0.0, 1.0);
	EmitVertex();

	gl_Position = vec4(vPos[2], 0.0, 1.0);
	EmitVertex();

	EndPrimitive();
}
