#pragma BLENDER_REQUIRE(common_globals_lib.glsl)
#pragma BLENDER_REQUIRE(common_overlay_lib.glsl)

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in float selectionFac[2];
flat in vec2 stippleStart[2];
noperspective in vec2 stipplePos[2];

uniform int lineStyle;
uniform bool doSmoothWire;

out float selectionFac_f;
noperspective out float edgeCoord_f;
noperspective out vec2 stipplePos_f;
flat out vec2 stippleStart_f;

void do_vertex(
    vec4 pos, float selection_fac, vec2 stipple_start, vec2 stipple_pos, float coord, vec2 offset)
{
  selectionFac_f = selection_fac;
  edgeCoord_f = coord;
  stippleStart_f = stipple_start;
  stipplePos_f = stipple_pos;

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
  /* Add 1 px for AA */
  if (doSmoothWire) {
    half_size += 0.5;
  }

  vec2 line = ss_pos[0] - ss_pos[1];
  vec2 line_dir = normalize(line);
  vec2 line_perp = vec2(-line_dir.y, line_dir.x);
  vec2 edge_ofs = line_perp * sizeViewportInv * ceil(half_size);

  do_vertex(pos0, selectionFac[0], stippleStart[0], stipplePos[0], half_size, edge_ofs.xy);
  do_vertex(pos0, selectionFac[0], stippleStart[0], stipplePos[0], -half_size, -edge_ofs.xy);
  do_vertex(pos1, selectionFac[1], stippleStart[1], stipplePos[1], half_size, edge_ofs.xy);
  do_vertex(pos1, selectionFac[1], stippleStart[1], stipplePos[1], -half_size, -edge_ofs.xy);

  EndPrimitive();
}
