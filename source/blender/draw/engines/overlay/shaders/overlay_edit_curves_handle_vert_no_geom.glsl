/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_math_lib.glsl"
#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"
#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 24)

#define M_TAN_PI_BY_8 tan(M_PI / 8)
#define M_TAN_3_PI_BY_8 tan(3 * M_PI / 8)
#define M_SQRT2_BY_2 (M_SQRT2 / 2)

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0); \
  finalColor = vec4(0.0); \
  return;

float4 get_bezier_handle_color(uint color_id, float sel)
{
  switch (color_id) {
    case 0u: /* BEZIER_HANDLE_FREE */
      return mix(globalsBlock.color_handle_free, globalsBlock.color_handle_sel_free, sel);
    case 1u: /* BEZIER_HANDLE_AUTO */
      return mix(globalsBlock.color_handle_auto, globalsBlock.color_handle_sel_auto, sel);
    case 2u: /* BEZIER_HANDLE_VECTOR */
      return mix(globalsBlock.color_handle_vect, globalsBlock.color_handle_sel_vect, sel);
    case 3u: /* BEZIER_HANDLE_ALIGN */
      return mix(globalsBlock.color_handle_align, globalsBlock.color_handle_sel_align, sel);
  }
  return mix(globalsBlock.color_handle_autoclamp, globalsBlock.color_handle_sel_autoclamp, sel);
}

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
  float vert_selection[2];

  /* Input prim is LineList. */
  /* Index of the input line primitive. */
  int input_line_id = gl_VertexID / 24;
  /* Index of output vertex set. Grouped into pairs as outputted by original "output_line" function
   * in overlay_edit_curve_handle_geom.glsl. */
  int output_quad_id = (gl_VertexID / 6) % 4;
  /* ID of vertex within generated line segment geometry. */
  int output_prim_vert_id = gl_VertexID % 24;
  int quotient = output_prim_vert_id / 3;
  int remainder = output_prim_vert_id % 3;
  int line_end_point = remainder == 0 || (quotient % 2 == 0 && remainder == 2) ? 0 : 1;

  for (int i = 0; i < 2; i++) {
    in_pos[i] = vertex_fetch_attribute((input_line_id * 2) + i, pos, vec3).xyz;
    vert_flag[i] = (uint)vertex_fetch_attribute((input_line_id * 2) + i, data, uchar);
    vert_selection[i] = (float)vertex_fetch_attribute((input_line_id * 2) + i, selection, float);
    world_pos[i] = point_object_to_world(in_pos[i]);
    ndc_pos[i] = point_world_to_ndc(world_pos[i]);
  }

  /* Perform Geometry shader equivalent calculation. */
  bool is_active = (vert_flag[0] & EDIT_CURVES_ACTIVE_HANDLE) != 0u;
  uint color_id = (vert_flag[0] >> EDIT_CURVES_HANDLE_TYPES_SHIFT) & 7;

  bool is_bezier_handle = (vert_flag[0] & EDIT_CURVES_BEZIER_HANDLE) != 0;
  /* Don't output any edges if we don't show handles */
  if ((uint(curveHandleDisplay) == CURVE_HANDLE_NONE) && is_bezier_handle) {
    DISCARD_VERTEX
    return;
  }

  /* If handle type is only selected and the edge is not selected, don't show.
   * Nurbs and other curves must show the handles always. */
  if ((uint(curveHandleDisplay) == CURVE_HANDLE_SELECTED) && is_bezier_handle && !is_active) {
    return;
  }

  vec4 inner_color;
  if ((vert_flag[line_end_point] & (EDIT_CURVES_BEZIER_HANDLE | EDIT_CURVES_BEZIER_KNOT)) != 0u) {
    inner_color = get_bezier_handle_color(color_id, vert_selection[line_end_point]);
  }
  else if ((vert_flag[line_end_point] & EDIT_CURVES_NURBS_CONTROL_POINT) != 0u) {
    inner_color = mix(globalsBlock.color_nurb_uline,
                      globalsBlock.color_nurb_sel_uline,
                      vert_selection[line_end_point]);
  }
  else {
    inner_color = mix(
        globalsBlock.color_wire, globalsBlock.color_vertex_select, vert_selection[line_end_point]);
  }

  vec4 outer_color = is_active ? mix(colorActiveSpline,
                                     inner_color,
                                     0.25) /* Minimize active color bleeding on inner_color. */
                                 :
                                 vec4(inner_color.rgb, 0.0);

  vec2 v1_2 = (ndc_pos[1].xy / ndc_pos[1].w - ndc_pos[0].xy / ndc_pos[0].w) * sizeViewport;
  vec2 offset = sizeEdge * 4.0 * sizeViewportInv; /* 4.0 is eyeballed */

  if (abs(v1_2.x) <= M_TAN_PI_BY_8 * abs(v1_2.y)) {
    offset.y = 0.0;
  }
  else if (abs(v1_2.x) <= M_TAN_3_PI_BY_8 * abs(v1_2.y)) {
    offset = offset * vec2(-M_SQRT2_BY_2 * sign(v1_2.x), M_SQRT2_BY_2 * sign(v1_2.y));
  }
  else {
    offset.x = 0.0;
  }

  /* Each output vertex falls into 10 possible positions to generate 8 output triangles between 5
   * lines. */
  /* Discard transparent border quads up-front. */
  if (!is_active) {
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
      offset *= 0.75; /* Don't make the active "halo" appear very thick. */
      output_vert(offset * -2.0, vec4(colorActiveSpline.rgb, 0.0), world_pos[0], ndc_pos[0]);
      break;
    }
    /* Bottom transparent border right. */
    case 23: {
      offset *= 0.75; /* Don't make the active "halo" appear very thick. */
      output_vert(offset * -2.0, vec4(colorActiveSpline.rgb, 0.0), world_pos[1], ndc_pos[1]);
      break;
    }
    default: {
      DISCARD_VERTEX
      break;
    }
  }
}
