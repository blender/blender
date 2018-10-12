#define EDGE_EXISTS     (1 << 0)
#define EDGE_ACTIVE     (1 << 1)
#define EDGE_SELECTED   (1 << 2)
#define EDGE_SEAM       (1 << 3)
#define EDGE_SHARP      (1 << 4)
#define EDGE_FREESTYLE  (1 << 5)
#define EDGE_VERTEX_ACTIVE   (1 << (0 + 8))
#define EDGE_VERTEX_SELECTED (1 << (1 + 8))
#define EDGE_VERTEX_EXISTS   (1 << (2 + 8))

#define VERTEX_ACTIVE   (1 << 0)
#define VERTEX_SELECTED (1 << 1)
#define VERTEX_EXISTS   (1 << 2)

#define FACE_ACTIVE     (1 << 3)
#define FACE_SELECTED   (1 << 4)
#define FACE_FREESTYLE  (1 << 5)

uniform bool doEdges = true;

vec4 EDIT_MESH_edge_color_outer(int edge_flag, bool face_active, float crease, float bweight)
{
	vec4 color = vec4(0.0);
	color = ((edge_flag & EDGE_FREESTYLE) != 0) ? colorEdgeFreestyle : color;
	color = ((edge_flag & EDGE_SHARP) != 0) ? colorEdgeSharp : color;
	color = (crease > 0.0) ? vec4(colorEdgeCrease.rgb, crease) : color;
	color = (bweight > 0.0) ? vec4(colorEdgeBWeight.rgb, bweight) : color;
	color = ((edge_flag & EDGE_SEAM) != 0) ? colorEdgeSeam : color;

	if (face_active) {
		color = colorEditMeshActive;
	}
	return color;
}

vec4 EDIT_MESH_edge_color_inner(int edge_flag, bool face_active)
{
	vec4 color = colorWireEdit;
#ifdef EDGE_SELECTION
	color = ((edge_flag & EDGE_SELECTED) != 0) ? colorEdgeSelect : color;
	color = ((edge_flag & EDGE_ACTIVE) != 0) ? colorEditMeshActive : color;
#else
	color = (doEdges && (edge_flag & EDGE_SELECTED) != 0) ? colorEdgeSelect : color;
#endif
	return color;
}

vec4 EDIT_MESH_vertex_color(int vertex_flag)
{
	vec4 color = colorWireEdit;
	color = (doEdges && (vertex_flag & (VERTEX_ACTIVE | VERTEX_SELECTED)) != 0) ? colorEdgeSelect : color;
	return color;
}
