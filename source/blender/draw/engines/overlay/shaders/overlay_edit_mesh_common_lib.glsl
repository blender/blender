/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

vec4 EDIT_MESH_edge_color_outer(uint edge_flag, uint face_flag, float crease, float bweight)
{
  vec4 color = vec4(0.0);
  color = ((edge_flag & EDGE_FREESTYLE) != 0u) ? colorEdgeFreestyle : color;
  color = ((edge_flag & EDGE_SHARP) != 0u) ? colorEdgeSharp : color;
  color = (crease > 0.0) ? vec4(colorEdgeCrease.rgb, crease) : color;
  color = (bweight > 0.0) ? vec4(colorEdgeBWeight.rgb, bweight) : color;
  color = ((edge_flag & EDGE_SEAM) != 0u) ? colorEdgeSeam : color;
  return color;
}

vec4 EDIT_MESH_edge_color_inner(uint edge_flag)
{
  vec4 color = colorWireEdit;
  vec4 color_select = (selectEdges) ? colorEdgeSelect : mix(colorEdgeSelect, colorWireEdit, .45);
  color = ((edge_flag & EDGE_SELECTED) != 0u) ? color_select : color;
  color = ((edge_flag & EDGE_ACTIVE) != 0u) ? colorEditMeshActive : color;

  color.a = (selectEdges || (edge_flag & (EDGE_SELECTED | EDGE_ACTIVE)) != 0u) ? 1.0 : 0.7;
  return color;
}

vec4 EDIT_MESH_edge_vertex_color(uint vertex_flag)
{
  vec4 color = colorWireEdit;
  vec4 color_select = (selectEdges) ? colorEdgeSelect : mix(colorEdgeSelect, colorWireEdit, .45);

  bool edge_selected = (vertex_flag & (VERT_ACTIVE | VERT_SELECTED)) != 0u;
  color = (edge_selected) ? color_select : color;

  color.a = (selectEdges || edge_selected) ? 1.0 : 0.7;
  return color;
}

vec4 EDIT_MESH_vertex_color(uint vertex_flag, float vertex_crease)
{
  if ((vertex_flag & VERT_ACTIVE) != 0u) {
    return vec4(colorEditMeshActive.xyz, 1.0);
  }
  else if ((vertex_flag & VERT_SELECTED) != 0u) {
    return colorVertexSelect;
  }
  else {
    /* Full crease color if not selected nor active. */
    if (vertex_crease > 0.0) {
      return mix(colorVertex, colorEdgeCrease, vertex_crease);
    }
    return colorVertex;
  }
}

vec4 EDIT_MESH_face_color(uint face_flag)
{
  vec4 color = colorFace;
  vec4 color_active = mix(colorFaceSelect, colorEditMeshActive, 0.5);
  color = (retopologyOffset > 0.0) ? colorFaceRetopology : color;
  color = ((face_flag & FACE_FREESTYLE) != 0u) ? colorFaceFreestyle : color;
  color = ((face_flag & FACE_SELECTED) != 0u) ? colorFaceSelect : color;
  color = ((face_flag & FACE_ACTIVE) != 0u) ? color_active : color;
  color.a *= ((face_flag & (FACE_FREESTYLE | FACE_SELECTED | FACE_ACTIVE)) == 0u || selectFaces) ?
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
