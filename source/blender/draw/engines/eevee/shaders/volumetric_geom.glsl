
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in vec4 vPos[];

flat out int slice;

/* This is just a pass-through geometry shader that send the geometry
 * to the layer corresponding to it's depth. */

void main() {
	gl_Layer = slice = int(vPos[0].z);

	gl_Position = vPos[0].xyww;
	EmitVertex();

	gl_Position = vPos[1].xyww;
	EmitVertex();

	gl_Position = vPos[2].xyww;
	EmitVertex();

	EndPrimitive();
}