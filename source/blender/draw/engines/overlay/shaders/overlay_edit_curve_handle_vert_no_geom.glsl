/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 24)

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0); \
  finalColor = vec4(0.0); \
  return;

void output_vert(vec2 offset, vec4 color, vec3 out_world_pos, vec4 out_ndc_pos)
{
  finalColor = color;
  gl_Position = out_ndc_pos;
  gl_Position.xy += offset * out_ndc_pos.w;
  view_clipping_distances(out_world_pos);
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  /* Perform vertex shader for each input primitive. */
  vec3 in_pos[2];
  vec3 world_pos[2];
  vec4 ndc_pos[2];
  uint vert_flag[2];

  /* Input prim is LineList. */
  /* Index of the input line primitive. */
  int input_line_id = gl_VertexID / 24;
  /* Index of output vertex set. Grouped into pairs as outputted by original "output_line" function
   * in overlay_edit_curve_handle_geom.glsl. */
  int output_quad_id = (gl_VertexID / 6) % 4;
  /* ID of vertex within generated line segment geometry. */
  int output_prim_vert_id = gl_VertexID % 24;

  for (int i = 0; i < 2; i++) {
    in_pos[i] = vertex_fetch_attribute((input_line_id * 2) + i, pos, vec3).xyz;
    vert_flag[i] = (uint)vertex_fetch_attribute((input_line_id * 2) + i, data, uchar);
    world_pos[i] = point_object_to_world(in_pos[i]);
    ndc_pos[i] = point_world_to_ndc(world_pos[i]);
  }

  /* Perform Geometry shader equivalent calculation. */
  uint is_active_nurb = (vert_flag[1] & ACTIVE_NURB);
  uint color_id = (vert_flag[1] >> COLOR_SHIFT);

  /* Don't output any edges if we don't show handles */
  if (!showCurveHandles && (color_id < 5u)) {
    DISCARD_VERTEX
    return;
  }

  bool edge_selected = (((vert_flag[1] | vert_flag[0]) & VERT_SELECTED) != 0u);
  bool handle_selected = (showCurveHandles &&
                          (((vert_flag[1] | vert_flag[0]) & VERT_SELECTED_BEZT_HANDLE) != 0u));

  bool is_gpencil = ((vert_flag[1] & VERT_GPENCIL_BEZT_HANDLE) != 0u);

  /* If handle type is only selected and the edge is not selected, don't show. */
  if ((uint(curveHandleDisplay) != CURVE_HANDLE_ALL) && (!handle_selected)) {
    /* Nurbs must show the handles always. */
    bool is_u_segment = (((vert_flag[1] ^ vert_flag[0]) & EVEN_U_BIT) != 0u);
    if ((!is_u_segment) && (color_id <= 4u)) {
      return;
    }
    if (is_gpencil) {
      return;
    }
  }

  vec4 inner_color;
  if (color_id == 0u) {
    inner_color = (edge_selected) ? colorHandleSelFree : colorHandleFree;
  }
  else if (color_id == 1u) {
    inner_color = (edge_selected) ? colorHandleSelAuto : colorHandleAuto;
  }
  else if (color_id == 2u) {
    inner_color = (edge_selected) ? colorHandleSelVect : colorHandleVect;
  }
  else if (color_id == 3u) {
    inner_color = (edge_selected) ? colorHandleSelAlign : colorHandleAlign;
  }
  else if (color_id == 4u) {
    inner_color = (edge_selected) ? colorHandleSelAutoclamp : colorHandleAutoclamp;
  }
  else {
    bool is_selected = (((vert_flag[1] & vert_flag[0]) & VERT_SELECTED) != 0u);
    bool is_u_segment = (((vert_flag[1] ^ vert_flag[0]) & EVEN_U_BIT) != 0u);
    if (is_u_segment) {
      inner_color = (is_selected) ? colorNurbSelUline : colorNurbUline;
    }
    else {
      inner_color = (is_selected) ? colorNurbSelVline : colorNurbVline;
    }
  }

  vec4 outer_color = (is_active_nurb != 0u) ?
                         mix(colorActiveSpline,
                             inner_color,
                             0.25) /* Minimize active color bleeding on inner_color. */
                         :
                         vec4(inner_color.rgb, 0.0);

  vec2 v1_2 = (ndc_pos[1].xy / ndc_pos[1].w - ndc_pos[0].xy / ndc_pos[0].w);
  vec2 offset = sizeEdge * 4.0 * sizeViewportInv; /* 4.0 is eyeballed */

  if (abs(v1_2.x * sizeViewport.x) < abs(v1_2.y * sizeViewport.y)) {
    offset.y = 0.0;
  }
  else {
    offset.x = 0.0;
  }

  /* Each output vertex falls into 10 possible positions to generate 8 output triangles between 5
   * lines. */
  /* Discard transparent border quads up-front. */
  if (!(is_active_nurb != 0u)) {
    if (output_quad_id == 0 || output_quad_id == 3) {
      DISCARD_VERTEX
      return;
    }
  }

  switch (output_prim_vert_id) {
    /* Top transparent border left (AA). */
    case 0: {
      offset *= 0.75; /* Don't make the active "halo" appear very thick. */
      output_vert(offset * 2.0, vec4(colorActiveSpline.rgb, 0.0), world_pos[0], ndc_pos[0]);
      break;
    }
    /* Top transparent border right (AA). */
    case 1:
    case 4: {
      offset *= 0.75; /* Don't make the active "halo" appear very thick. */
      output_vert(offset * 2.0, vec4(colorActiveSpline.rgb, 0.0), world_pos[1], ndc_pos[1]);
      break;
    }
    /* Top Outline row left point. */
    case 2:
    case 3:
    case 6: {
      output_vert(offset, outer_color, world_pos[0], ndc_pos[0]);
      break;
    }
    /* Top Outline row right point. */
    case 5:
    case 7:
    case 10: {
      output_vert(offset, outer_color, world_pos[1], ndc_pos[1]);
      break;
    }
    /* Core line left point. */
    case 8:
    case 9:
    case 12: {
      output_vert(vec2(0.0), inner_color, world_pos[0], ndc_pos[0]);
      break;
    }
    /* Core line right point. */
    case 11:
    case 13:
    case 16: {
      output_vert(vec2(0.0), inner_color, world_pos[1], ndc_pos[1]);
      break;
    }
    /* Bottom outline left point. */
    case 14:
    case 15:
    case 18: {
      output_vert(-offset, outer_color, world_pos[0], ndc_pos[0]);
      break;
    }
    /* Bottom outline right point. */
    case 17:
    case 19:
    case 22: {
      output_vert(-offset, outer_color, world_pos[1], ndc_pos[1]);
      break;
    }
    /* Bottom transparent border left. */
    case 20:
    case 21: {
      output_vert(offset * -2.0, vec4(colorActiveSpline.rgb, 0.0), world_pos[0], ndc_pos[0]);
    }
    /* Bottom transparent border right. */
    case 23: {
      output_vert(offset * -2.0, vec4(colorActiveSpline.rgb, 0.0), world_pos[1], ndc_pos[1]);
    }
    default: {
      DISCARD_VERTEX
      break;
    }
  }
}
