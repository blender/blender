/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 6)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(overlay_common_lib.glsl)

#define DISCARD_VERTEX \
  gl_Position = vec4(0.0); \
  return;

void do_vertex(
    vec4 pos, float selection_fac, vec2 stipple_start, vec2 stipple_pos, float coord, vec2 offset)
{
  geom_out.selectionFac = selection_fac;
  geom_noperspective_out.edgeCoord = coord;
  geom_flat_out.stippleStart = stipple_start;
  geom_noperspective_out.stipplePos = stipple_pos;

  gl_Position = pos;
  /* Multiply offset by 2 because gl_Position range is [-1..1]. */
  gl_Position.xy += offset * 2.0;
}

void main()
{
  /* Determine output geometry parameters. */
  int quad_id = gl_VertexID / 6;
  int quad_vertex_id = gl_VertexID % 6;
  int base_vertex_id = 0;
  if (vertex_fetch_get_input_prim_type() == GPU_PRIM_LINES) {
    base_vertex_id = quad_id * 2;
  }
  else if (vertex_fetch_get_input_prim_type() == GPU_PRIM_LINE_STRIP) {
    base_vertex_id = quad_id;
  }
  else {
    DISCARD_VERTEX
  }

  /* Read vertex attributes for line prims. */
  vec2 root_au0 = vertex_fetch_attribute(base_vertex_id, au, vec2);
  vec2 root_au1 = vertex_fetch_attribute(base_vertex_id + 1, au, vec2);
  int root_flag0 = vertex_fetch_attribute(base_vertex_id, flag, int);
  int root_flag1 = vertex_fetch_attribute(base_vertex_id + 1, flag, int);

  /* Vertex shader per input vertex. */
  vec3 world_pos0 = point_object_to_world(vec3(root_au0, 0.0));
  vec3 world_pos1 = point_object_to_world(vec3(root_au1, 0.0));
  vec4 ndc_pos0 = point_world_to_ndc(world_pos0);
  vec4 ndc_pos1 = point_world_to_ndc(world_pos1);

  /* Snap vertices to the pixel grid to reduce artifacts. */
  vec2 half_viewport_res = sizeViewport * 0.5;
  vec2 half_pixel_offset = sizeViewportInv * 0.5;
  ndc_pos0.xy = floor(ndc_pos0.xy * half_viewport_res) / half_viewport_res + half_pixel_offset;
  ndc_pos1.xy = floor(ndc_pos1.xy * half_viewport_res) / half_viewport_res + half_pixel_offset;

#ifdef USE_EDGE_SELECT
  bool is_select0 = (root_flag0 & EDGE_UV_SELECT) != 0;
  bool is_select1 = (root_flag1 & EDGE_UV_SELECT) != 0;
#else
  bool is_select0 = (root_flag0 & VERT_UV_SELECT) != 0;
  bool is_select1 = (root_flag1 & VERT_UV_SELECT) != 0;
#endif

  float selectionFac0 = is_select0 ? 1.0 : 0.0;  // out float selectionFac;
  float selectionFac1 = is_select1 ? 1.0 : 0.0;  // out float selectionFac;
#ifdef USE_EDGE_SELECT
  /* No blending with edge selection. */
  selectionFac1 = selectionFac0;
#endif

  /* Move selected edges to the top
   * Vertices are between 0.0 and 0.2, Edges between 0.2 and 0.4
   * actual pixels are at 0.75, 1.0 is used for the background. */
  float depth0 = is_select0 ? 0.25 : 0.35;
  float depth1 = is_select1 ? 0.25 : 0.35;
  ndc_pos0.z = depth0;
  ndc_pos1.z = depth1;

  /* Avoid precision loss. */
  vec2 stipplePos0 = 500.0 + 500.0 * (ndc_pos0.xy / ndc_pos0.w);
  vec2 stipplePos1 = 500.0 + 500.0 * (ndc_pos1.xy / ndc_pos1.w);
  vec2 stippleStart0 = stipplePos0;
  vec2 stippleStart1 = stipplePos1;

  /* Geometry shader equivalent calculations. */
  vec2 ss_pos[2];
  ss_pos[0] = ndc_pos0.xy / ndc_pos0.w;
  ss_pos[1] = ndc_pos1.xy / ndc_pos1.w;

  float half_size = sizeEdge;

  /* Enlarge edge for outline drawing. */
  /* Factor of 3.0 out of nowhere! Seems to fix issues with float imprecision. */
  half_size += (lineStyle == OVERLAY_UV_LINE_STYLE_OUTLINE) ?
                   max(sizeEdge * (doSmoothWire ? 1.0 : 3.0), 1.0) :
                   0.0;
  /* Add 1 px for AA */
  if (doSmoothWire) {
    half_size += 0.5;
  }

  vec2 line = ss_pos[0] - ss_pos[1];
  vec2 line_dir = normalize(line);
  vec2 line_perp = vec2(-line_dir.y, line_dir.x);
  vec2 edge_ofs = line_perp * sizeViewportInv * ceil(half_size);

  switch (quad_vertex_id) {
    case 1: /* vertex A */
    case 3:
      do_vertex(ndc_pos1, selectionFac1, stippleStart1, stipplePos1, half_size, edge_ofs.xy);
      break;
    case 0: /* B */
      do_vertex(ndc_pos0, selectionFac0, stippleStart0, stipplePos0, half_size, edge_ofs.xy);
      break;

    case 2: /* C */
    case 4:
      do_vertex(ndc_pos0, selectionFac0, stippleStart0, stipplePos0, -half_size, -edge_ofs.xy);
      break;

    case 5: /* D */
      do_vertex(ndc_pos1, selectionFac1, stippleStart1, stipplePos1, -half_size, -edge_ofs.xy);
      break;
  }
}
