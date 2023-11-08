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
  vec4 selected_edge_col = (selectEdge) ? colorEdgeModeSelect : colorEdgeSelect;
  color = ((edge_flag & EDGE_SELECTED) != 0u) ? selected_edge_col : color;
  color = ((edge_flag & EDGE_ACTIVE) != 0u) ? colorEditMeshActive : color;
  color.a = 1.0;
  return color;
}

vec4 EDIT_MESH_edge_vertex_color(uint vertex_flag)
{
  /* Edge color in vertex selection mode. */
  vec4 selected_edge_col = (selectEdge) ? colorEdgeModeSelect : colorEdgeSelect;
  bool edge_selected = (vertex_flag & (VERT_ACTIVE | VERT_SELECTED)) != 0u;
  vec4 color = (edge_selected) ? selected_edge_col : colorWireEdit;
  color.a = 1.0;
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
  bool face_freestyle = (face_flag & FACE_FREESTYLE) != 0u;
  bool face_selected = (face_flag & FACE_SELECTED) != 0u;
  bool face_active = (face_flag & FACE_ACTIVE) != 0u;
  bool face_retopo = (retopologyOffset > 0.0);
  vec4 selected_face_col = (selectFace) ? colorFaceModeSelect : colorFaceSelect;
  vec4 color = colorFace;
  color = face_retopo ? colorFaceRetopology : color;
  color = face_freestyle ? colorFaceFreestyle : color;
  color = face_selected ? selected_face_col : color;
  if (selectFace && face_active) {
    color = mix(selected_face_col, colorEditMeshActive, 0.5);
    color.a = selected_face_col.a;
  }
  if (wireShading) {
    /* Lower face selection opacity for better wireframe visibility. */
    color.a = (face_selected) ? color.a * 0.6 : color.a;
  }
  else {
    /* Don't always fill 'colorFace'. */
    color.a = (selectFace || face_selected || face_active || face_freestyle || face_retopo) ?
                  color.a :
                  0.0;
  }
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
