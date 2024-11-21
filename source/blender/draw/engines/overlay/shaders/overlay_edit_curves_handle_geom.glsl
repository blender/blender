/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_math_lib.glsl"
#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"

#define M_TAN_PI_BY_8 tan(M_PI / 8)
#define M_TAN_3_PI_BY_8 tan(3 * M_PI / 8)
#define M_SQRT2_BY_2 (M_SQRT2 / 2)

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

void output_line(vec2 offset, vec4 color_start, vec4 color_end)
{
  finalColor = color_start;

  gl_Position = gl_in[0].gl_Position;
  gl_Position.xy += offset * gl_in[0].gl_Position.w;
  view_clipping_distances_set(gl_in[0]);
  gpu_EmitVertex();

  finalColor = color_end;

  gl_Position = gl_in[1].gl_Position;
  gl_Position.xy += offset * gl_in[1].gl_Position.w;
  view_clipping_distances_set(gl_in[1]);
  gpu_EmitVertex();
}

void main()
{

  vec4 v1 = gl_in[0].gl_Position;
  vec4 v2 = gl_in[1].gl_Position;

  bool is_active = (vert[0].flag & EDIT_CURVES_ACTIVE_HANDLE) != 0u;
  uint color_id = uint(vert[0].flag >> EDIT_CURVES_HANDLE_TYPES_SHIFT) & 7;

  bool is_bezier_handle = (vert[0].flag & EDIT_CURVES_BEZIER_HANDLE) != 0;
  /* Don't output any edges if we don't show handles */
  if ((uint(curveHandleDisplay) == CURVE_HANDLE_NONE) && is_bezier_handle) {
    return;
  }

  /* If handle type is only selected and the edge is not selected, don't show.
   * Nurbs and other curves must show the handles always. */
  if ((uint(curveHandleDisplay) == CURVE_HANDLE_SELECTED) && is_bezier_handle && !is_active) {
    return;
  }

  vec4 inner_color[2];
  if ((vert[0].flag & (EDIT_CURVES_BEZIER_HANDLE | EDIT_CURVES_BEZIER_KNOT)) != 0u) {
    inner_color[0] = get_bezier_handle_color(color_id, vert[0].selection);
    inner_color[1] = get_bezier_handle_color(color_id, vert[1].selection);
  }
  else if ((vert[0].flag & EDIT_CURVES_NURBS_CONTROL_POINT) != 0u) {
    inner_color[0] = mix(
        globalsBlock.color_nurb_uline, globalsBlock.color_nurb_sel_uline, vert[0].selection);
    inner_color[1] = mix(
        globalsBlock.color_nurb_uline, globalsBlock.color_nurb_sel_uline, vert[1].selection);
  }
  else {
    inner_color[0] = mix(
        globalsBlock.color_wire, globalsBlock.color_vertex_select, vert[0].selection);
    inner_color[1] = mix(
        globalsBlock.color_wire, globalsBlock.color_vertex_select, vert[1].selection);
  }

  vec4 outer_color[2];
  if (is_active) {
    outer_color[0] = mix(colorActiveSpline,
                         inner_color[0],
                         0.25); /* Minimize active color bleeding on inner_color. */
    outer_color[1] = mix(colorActiveSpline,
                         inner_color[1],
                         0.25); /* Minimize active color bleeding on inner_color. */
  }
  else {
    outer_color[0] = vec4(inner_color[0].rgb, 0.0);
    outer_color[1] = vec4(inner_color[1].rgb, 0.0);
  }

  vec2 v1_2 = (v2.xy / v2.w - v1.xy / v1.w) * sizeViewport;
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

  /* draw the transparent border (AA). */
  if (is_active) {
    offset *= 0.75; /* Don't make the active "halo" appear very thick. */
    output_line(offset * 2.0, vec4(colorActiveSpline.rgb, 0.0), vec4(colorActiveSpline.rgb, 0.0));
  }

  /* draw the outline. */
  output_line(offset, outer_color[0], outer_color[1]);

  /* draw the core of the line. */
  output_line(vec2(0.0), inner_color[0], inner_color[1]);

  /* draw the outline. */
  output_line(-offset, outer_color[0], outer_color[1]);

  /* draw the transparent border (AA). */
  if (is_active) {
    output_line(offset * -2.0, vec4(colorActiveSpline.rgb, 0.0), vec4(colorActiveSpline.rgb, 0.0));
  }

  EndPrimitive();
}
