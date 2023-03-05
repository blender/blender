
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma USE_SSBO_VERTEX_FETCH(TriangleStrip, 10)

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0); \
  finalColor = vec4(0.0); \
  return;

void output_line(vec2 offset, vec4 color, vec3 out_world_pos, vec4 out_ndc_pos)
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
  int input_line_id = gl_VertexID / 10;
  /* Index of output vertex set. Grouped into pairs as outputted by original "output_line" function
   * in overlay_edit_curve_handle_geom.glsl. */
  int output_prim_id = (gl_VertexID / 2) % 5;
  /* ID of vertex within line primitive (0 or 1) for current vertex. */
  int output_prim_vert_id = gl_VertexID % 2;

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
  if (!showCurveHandles && (color_id < 5)) {
    return;
  }

  bool edge_selected = (((vert_flag[1] | vert_flag[0]) & VERT_SELECTED) != 0u);
  bool handle_selected = (showCurveHandles &&
                          (((vert_flag[1] | vert_flag[0]) & VERT_SELECTED_BEZT_HANDLE) != 0u));

  bool is_gpencil = ((vert_flag[1] & VERT_GPENCIL_BEZT_HANDLE) != 0u);

  /* If handle type is only selected and the edge is not selected, don't show. */
  if ((curveHandleDisplay != CURVE_HANDLE_ALL) && (!handle_selected)) {
    /* Nurbs must show the handles always. */
    bool is_u_segment = (((vert_flag[1] ^ vert_flag[0]) & EVEN_U_BIT) != 0u);
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
    bool is_selected = (((vert_flag[1] & vert_flag[0]) & VERT_SELECTED) != 0);
    bool is_u_segment = (((vert_flag[1] ^ vert_flag[0]) & EVEN_U_BIT) != 0);
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

  vec2 v1_2 = (ndc_pos[1].xy / ndc_pos[1].w - ndc_pos[0].xy / ndc_pos[0].w);
  vec2 offset = sizeEdge * 4.0 * sizeViewportInv; /* 4.0 is eyeballed */

  if (abs(v1_2.x * sizeViewport.x) < abs(v1_2.y * sizeViewport.y)) {
    offset.y = 0.0;
  }
  else {
    offset.x = 0.0;
  }

  /* Output geometry based on output line ID. */
  switch (output_prim_id) {
    case 0: {
      /* draw the transparent border (AA). */
      if (is_active_nurb != 0u) {
        offset *= 0.75; /* Don't make the active "halo" appear very thick. */
        output_line(offset * 2.0,
                    vec4(colorActiveSpline.rgb, 0.0),
                    world_pos[output_prim_vert_id],
                    ndc_pos[output_prim_vert_id]);
      }
      else {
        DISCARD_VERTEX
      }
      break;
    }
    case 1: {
      /* draw the outline. */
      output_line(
          offset, outer_color, world_pos[output_prim_vert_id], ndc_pos[output_prim_vert_id]);
      break;
    }
    case 2: {
      /* draw the core of the line. */
      output_line(
          vec2(0.0), inner_color, world_pos[output_prim_vert_id], ndc_pos[output_prim_vert_id]);
      break;
    }
    case 3: {
      /* draw the outline. */
      output_line(
          -offset, outer_color, world_pos[output_prim_vert_id], ndc_pos[output_prim_vert_id]);
      break;
    }
    case 4: {
      /* draw the transparent border (AA). */
      if (is_active_nurb != 0u) {
        output_line(offset * -2.0,
                    vec4(colorActiveSpline.rgb, 0.0),
                    world_pos[output_prim_vert_id],
                    ndc_pos[output_prim_vert_id]);
      }
      break;
    }

    default: {
      DISCARD_VERTEX
      break;
    }
  }
}
