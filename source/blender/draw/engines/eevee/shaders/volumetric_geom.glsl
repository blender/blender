
#ifdef MESH_SHADER
/* TODO tight slices */
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;
#else /* World */
layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;
#endif

in vec4 vPos[];

flat out int slice;

#ifdef MESH_SHADER
/* TODO tight slices */
void main() {
	gl_Layer = slice = int(vPos[0].z);

#ifdef ATTRIB
	pass_attrib(0);
#endif
	gl_Position = vPos[0].xyww;
	EmitVertex();

#ifdef ATTRIB
	pass_attrib(1);
#endif
	gl_Position = vPos[1].xyww;
	EmitVertex();

#ifdef ATTRIB
	pass_attrib(2);
#endif
	gl_Position = vPos[2].xyww;
	EmitVertex();

	EndPrimitive();
}

#else /* World */

/* This is just a pass-through geometry shader that send the geometry
 * to the layer corresponding to it's depth. */

void main() {
	gl_Layer = slice = int(vPos[0].z);

#ifdef ATTRIB
	pass_attrib(0);
#endif
	gl_Position = vPos[0].xyww;
	EmitVertex();

#ifdef ATTRIB
	pass_attrib(1);
#endif
	gl_Position = vPos[1].xyww;
	EmitVertex();

#ifdef ATTRIB
	pass_attrib(2);
#endif
	gl_Position = vPos[2].xyww;
	EmitVertex();

	EndPrimitive();
}

#endif
