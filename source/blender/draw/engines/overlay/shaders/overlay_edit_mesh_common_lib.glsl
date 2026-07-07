/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/overlay_edit_mode_infos.hh"

SHADER_LIBRARY_CREATE_INFO(overlay_edit_mesh_common)

float4 EDIT_MESH_edge_color_outer(uint edge_flag, uint face_flag, float crease, float bweight)
{
  float4 color = float4(0.0f);
  color = ((edge_flag & EDGE_FREESTYLE) != 0u) ? theme.colors.edge_freestyle : color;
  color = ((edge_flag & EDGE_SHARP) != 0u) ? theme.colors.edge_sharp : color;
  color = (crease > 0.0f) ? float4(theme.colors.edge_crease.rgb, crease) : color;
  color = (bweight > 0.0f) ? float4(theme.colors.edge_bweight.rgb, bweight) : color;
  color = ((edge_flag & EDGE_SEAM) != 0u) ? theme.colors.edge_seam : color;
  return color;
}

float4 EDIT_MESH_edge_color_inner(uint edge_flag)
{
  float4 color = theme.colors.wire_edit;
  float4 selected_edge_col = (select_edge) ? theme.colors.edge_mode_select :
                                             theme.colors.edge_select;
  color = ((edge_flag & EDGE_SELECTED) != 0u) ? selected_edge_col : color;
  color = ((edge_flag & EDGE_ACTIVE) != 0u) ? theme.colors.edit_mesh_active : color;
  color.a = 1.0f;
  return color;
}

float4 EDIT_MESH_edge_vertex_color(uint vertex_flag)
{
  /* Edge color in vertex selection mode. */
  float4 selected_edge_col = (select_edge) ? theme.colors.edge_mode_select :
                                             theme.colors.edge_select;
  bool edge_selected = (vertex_flag & (VERT_ACTIVE | VERT_SELECTED)) != 0u;
  float4 color = (edge_selected) ? selected_edge_col : theme.colors.wire_edit;
  color.a = 1.0f;
  return color;
}

float4 EDIT_MESH_vertex_color(uint vertex_flag, float vertex_crease)
{
  if ((vertex_flag & VERT_ACTIVE) != 0u) {
    return float4(theme.colors.edit_mesh_active.xyz, 1.0f);
  }
  else if ((vertex_flag & VERT_SELECTED) != 0u) {
    return theme.colors.vert_select;
  }
  else {
    /* Full crease color if not selected nor active. */
    if (vertex_crease > 0.0f) {
      return mix(theme.colors.vert, theme.colors.edge_crease, vertex_crease);
    }
    return theme.colors.vert;
  }
}

float4 EDIT_MESH_face_color(uint face_flag)
{
  bool face_freestyle = (face_flag & FACE_FREESTYLE) != 0u;
  bool face_selected = (face_flag & FACE_SELECTED) != 0u;
  bool face_active = (face_flag & FACE_ACTIVE) != 0u;
  bool face_retopo = (retopology_offset > 0.0f);
  float4 selected_face_col = (select_face) ? theme.colors.face_mode_select :
                                             theme.colors.face_select;
  float4 color = theme.colors.face;
  color = face_retopo ? theme.colors.face_retopology : color;
  color = face_freestyle ? theme.colors.face_freestyle : color;
  color = face_selected ? selected_face_col : color;
  if (select_face && face_active) {
    color = mix(selected_face_col, theme.colors.edit_mesh_active, 0.5f);
    color.a = selected_face_col.a;
  }
  if (wire_shading) {
    /* Lower face selection opacity for better wireframe visibility. */
    color.a = (face_selected) ? color.a * 0.6f : color.a;
  }
  else {
    /* Don't always fill 'theme.colors.face'. */
    color.a = (select_face || face_selected || face_active || face_freestyle || face_retopo) ?
                  color.a :
                  0.0f;
  }
  return color;
}

float4 EDIT_MESH_facedot_color(float facedot_flag)
{
  if (facedot_flag < 0.0f) {
    return float4(theme.colors.edit_mesh_active.xyz, 1.0f);
  }
  else if (facedot_flag > 0.0f) {
    return theme.colors.facedot;
  }
  else {
    return theme.colors.vert;
  }
}
