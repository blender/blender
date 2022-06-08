
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void output_line(vec2 offset, vec4 color)
{
  finalColor = color;

  gl_Position = gl_in[0].gl_Position;
  gl_Position.xy += offset * gl_in[0].gl_Position.w;
  view_clipping_distances_set(gl_in[0]);
  EmitVertex();

  gl_Position = gl_in[1].gl_Position;
  gl_Position.xy += offset * gl_in[1].gl_Position.w;
  view_clipping_distances_set(gl_in[1]);
  EmitVertex();
}

void main()
{
  vec4 v1 = gl_in[0].gl_Position;
  vec4 v2 = gl_in[1].gl_Position;

  int is_active_nurb = (vert[1].flag & ACTIVE_NURB);
  int color_id = (vert[1].flag >> COLOR_SHIFT);

  /* Don't output any edges if we don't show handles */
  if (!showCurveHandles && (color_id < 5)) {
    return;
  }

  bool edge_selected = (((vert[1].flag | vert[0].flag) & VERT_SELECTED) != 0);
  bool handle_selected = (showCurveHandles &&
                          (((vert[1].flag | vert[0].flag) & VERT_SELECTED_BEZT_HANDLE) != 0));

  bool is_gpencil = ((vert[1].flag & VERT_GPENCIL_BEZT_HANDLE) != 0);

  /* If handle type is only selected and the edge is not selected, don't show. */
  if ((curveHandleDisplay != CURVE_HANDLE_ALL) && (!handle_selected)) {
    /* Nurbs must show the handles always. */
    bool is_u_segment = (((vert[1].flag ^ vert[0].flag) & EVEN_U_BIT) != 0);
    if ((!is_u_segment) && (color_id <= 4)) {
      return;
    }
    if (is_gpencil) {
      return;
    }
  }

  vec4 inner_color;
  if (color_id == 0) {
    inner_color = (edge_selected) ? colorHandleSelFree : colorHandleFree;
  }
  else if (color_id == 1) {
    inner_color = (edge_selected) ? colorHandleSelAuto : colorHandleAuto;
  }
  else if (color_id == 2) {
    inner_color = (edge_selected) ? colorHandleSelVect : colorHandleVect;
  }
  else if (color_id == 3) {
    inner_color = (edge_selected) ? colorHandleSelAlign : colorHandleAlign;
  }
  else if (color_id == 4) {
    inner_color = (edge_selected) ? colorHandleSelAutoclamp : colorHandleAutoclamp;
  }
  else {
    bool is_selected = (((vert[1].flag & vert[0].flag) & VERT_SELECTED) != 0);
    bool is_u_segment = (((vert[1].flag ^ vert[0].flag) & EVEN_U_BIT) != 0);
    if (is_u_segment) {
      inner_color = (is_selected) ? colorNurbSelUline : colorNurbUline;
    }
    else {
      inner_color = (is_selected) ? colorNurbSelVline : colorNurbVline;
    }
  }

  vec4 outer_color = (is_active_nurb != 0) ?
                         mix(colorActiveSpline,
                             inner_color,
                             0.25) /* Minimize active color bleeding on inner_color. */
                         :
                         vec4(inner_color.rgb, 0.0);

  vec2 v1_2 = (v2.xy / v2.w - v1.xy / v1.w);
  vec2 offset = sizeEdge * 4.0 * drw_view.viewport_size_inverse; /* 4.0 is eyeballed */

  if (abs(v1_2.x * sizeViewport.x) < abs(v1_2.y * sizeViewport.y)) {
    offset.y = 0.0;
  }
  else {
    offset.x = 0.0;
  }

  /* draw the transparent border (AA). */
  if (is_active_nurb != 0) {
    offset *= 0.75; /* Don't make the active "halo" appear very thick. */
    output_line(offset * 2.0, vec4(colorActiveSpline.rgb, 0.0));
  }

  /* draw the outline. */
  output_line(offset, outer_color);

  /* draw the core of the line. */
  output_line(vec2(0.0), inner_color);

  /* draw the outline. */
  output_line(-offset, outer_color);

  /* draw the transparent border (AA). */
  if (is_active_nurb != 0) {
    output_line(offset * -2.0, vec4(colorActiveSpline.rgb, 0.0));
  }

  EndPrimitive();
}
