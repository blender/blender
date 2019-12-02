
uniform bool selectFaces = true;
uniform bool selectEdges = true;

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
  vec4 color_select = (selectEdges) ? colorEdgeSelect : mix(colorEdgeSelect, colorWireEdit, .45);
  color = ((edge_flag & EDGE_SELECTED) != 0) ? color_select : color;
  color = ((edge_flag & EDGE_ACTIVE) != 0) ? colorEditMeshActive : color;

  color.a = (selectEdges || (edge_flag & (EDGE_SELECTED | EDGE_ACTIVE)) != 0) ? 1.0 : 0.4;
  return color;
}

vec4 EDIT_MESH_edge_vertex_color(int vertex_flag)
{
  vec4 color = colorWireEdit;
  vec4 color_select = (selectEdges) ? colorEdgeSelect : mix(colorEdgeSelect, colorWireEdit, .45);

  bool edge_selected = (vertex_flag & (VERT_ACTIVE | VERT_SELECTED)) != 0;
  color = (edge_selected) ? color_select : color;

  color.a = (selectEdges || edge_selected) ? 1.0 : 0.4;
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
  vec4 color = colorFace;
  vec4 color_active = mix(colorFaceSelect, colorEditMeshActive, 0.5);
  color = ((face_flag & FACE_FREESTYLE) != 0) ? colorFaceFreestyle : color;
  color = ((face_flag & FACE_SELECTED) != 0) ? colorFaceSelect : color;
  color = ((face_flag & FACE_ACTIVE) != 0) ? color_active : color;
  color.a *= ((face_flag & (FACE_FREESTYLE | FACE_SELECTED | FACE_ACTIVE)) == 0 || selectFaces) ?
                 1.0 :
                 0.5;
  return color;
}

vec4 EDIT_MESH_facedot_color(float facedot_flag)
{
  if (facedot_flag < 0.0f) {
    return vec4(colorEditMeshActive.xyz, 1.0);
  }
  else if (facedot_flag > 0.0f) {
    return colorFaceDot;
  }
  else {
    return colorVertex;
  }
}
