#pragma BLENDER_REQUIRE(overlay_common_lib.glsl)

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
  EmitVertex();
}

void main()
{
  vec2 ss_pos[2];
  vec4 pos0 = gl_in[0].gl_Position;
  vec4 pos1 = gl_in[1].gl_Position;
  ss_pos[0] = pos0.xy / pos0.w;
  ss_pos[1] = pos1.xy / pos1.w;

  float half_size = sizeEdge;
  /* Enlarge edge for outline drawing. */
  /* Factor of 3.0 out of nowhere! Seems to fix issues with float imprecision. */
  half_size += (lineStyle == OVERLAY_UV_LINE_STYLE_OUTLINE) ?
                   max(sizeEdge * (doSmoothWire ? 1.0 : 3.0), 1.0) :
                   0.0;
  /* Add 1 PX for AA. */
  if (doSmoothWire) {
    half_size += 0.5;
  }

  vec2 line = ss_pos[0] - ss_pos[1];
  vec2 line_dir = normalize(line);
  vec2 line_perp = vec2(-line_dir.y, line_dir.x);
  vec2 edge_ofs = line_perp * sizeViewportInv * ceil(half_size);
  float selectFac0 = geom_in[0].selectionFac;
  float selectFac1 = geom_in[1].selectionFac;
#ifdef USE_EDGE_SELECT
  /* No blending with edge selection. */
  selectFac1 = selectFac0;
#endif

  do_vertex(pos0,
            selectFac0,
            geom_flat_in[0].stippleStart,
            geom_noperspective_in[0].stipplePos,
            half_size,
            edge_ofs.xy);
  do_vertex(pos0,
            selectFac0,
            geom_flat_in[0].stippleStart,
            geom_noperspective_in[0].stipplePos,
            -half_size,
            -edge_ofs.xy);
  do_vertex(pos1,
            selectFac1,
            geom_flat_in[1].stippleStart,
            geom_noperspective_in[1].stipplePos,
            half_size,
            edge_ofs.xy);
  do_vertex(pos1,
            selectFac1,
            geom_flat_in[1].stippleStart,
            geom_noperspective_in[1].stipplePos,
            -half_size,
            -edge_ofs.xy);

  EndPrimitive();
}
