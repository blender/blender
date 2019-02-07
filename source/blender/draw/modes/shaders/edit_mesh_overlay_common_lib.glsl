
uniform bool doEdges = true;

vec4 EDIT_MESH_edge_color_outer(int edge_flag, int face_flag, float crease, float bweight)
{
	vec4 color = vec4(0.0);
	color = ((edge_flag & EDGE_FREESTYLE) != 0) ? colorEdgeFreestyle : color;
	color = ((edge_flag & EDGE_SHARP) != 0) ? colorEdgeSharp : color;
	color = (crease > 0.0) ? vec4(colorEdgeCrease.rgb, crease) : color;
	color = (bweight > 0.0) ? vec4(colorEdgeBWeight.rgb, bweight) : color;
	color = ((edge_flag & EDGE_SEAM) != 0) ? colorEdgeSeam : color;
	return color;
}

vec4 EDIT_MESH_edge_color_inner(int edge_flag)
{
	vec4 color = colorWireEdit;
	color = ((edge_flag & EDGE_SELECTED) != 0) ? colorEdgeSelect : color;
	color = ((edge_flag & EDGE_ACTIVE) != 0) ? colorEditMeshActive : color;
	return color;
}

vec4 EDIT_MESH_edge_vertex_color(int vertex_flag)
{
	vec4 color = colorWireEdit;
	color = (doEdges && (vertex_flag & (VERT_ACTIVE | VERT_SELECTED)) != 0) ? colorEdgeSelect : color;
	return color;
}

vec4 EDIT_MESH_vertex_color(int vertex_flag)
{
	if ((vertex_flag & VERT_ACTIVE) != 0) {
		return vec4(colorEditMeshActive.xyz, 1.0);
	}
	else if ((vertex_flag & VERT_SELECTED) != 0) {
		return colorVertexSelect;
	}
	else {
		return colorVertex;
	}
}

vec4 EDIT_MESH_face_color(int face_flag)
{
	if ((face_flag & FACE_ACTIVE) != 0) {
		return mix(colorFaceSelect, colorEditMeshActive, 0.5);
	}
	else if ((face_flag & FACE_SELECTED) != 0) {
		return colorFaceSelect;
	}
	else if ((face_flag & FACE_FREESTYLE) != 0) {
		return colorFaceFreestyle;
	}
	else {
		return colorFace;
	}
}

vec4 EDIT_MESH_facedot_color(float facedot_flag)
{
	if (facedot_flag != 0.0) {
		return colorFaceDot;
	}
	else {
		return colorVertex;
	}
}
