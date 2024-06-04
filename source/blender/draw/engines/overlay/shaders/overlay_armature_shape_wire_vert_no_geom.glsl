/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma USE_SSBO_VERTEX_FETCH(TriangleList, 6)
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

vec4 vertex_main(mat4 model_mat, vec3 in_pos)
{
  vec3 world_pos = (model_mat * vec4(in_pos, 1.0)).xyz;
  view_clipping_distances(world_pos);
  return point_world_to_ndc(world_pos);
}

void do_vertex(vec4 pos, vec2 ofs, float coord, float flip)
{
  geometry_noperspective_out.edgeCoord = coord;
  gl_Position = pos;
  /* Multiply offset by 2 because gl_Position range is [-1..1]. */
  gl_Position.xy += flip * ofs * 2.0 * pos.w;
}

void main()
{
  /* Fetch per-instance data (matrix) and extract data from it. */
  mat4 in_inst_obmat = vertex_fetch_attribute(gl_VertexID, inst_obmat, mat4);
  vec4 bone_color, state_color;
  mat4 model_mat = extract_matrix_packed_data(in_inst_obmat, state_color, bone_color);

  geometry_out.finalColor.rgb = mix(state_color.rgb, bone_color.rgb, 0.5);
  geometry_out.finalColor.a = 1.0;
  /* Because the packing clamps the value, the wire width is passed in compressed.
  `sizeEdge` is defined as the distance from the center to the outer edge. As such to get the total
   width it needs to be doubled. */
  float wire_width = bone_color.a * WIRE_WIDTH_COMPRESSION * (sizeEdge * 2);
  geometry_out.wire_width = wire_width;

  /* Fetch vertex positions and transform to clip space ("vertex shader"). */
  int quad_id = gl_VertexID / 6;
  int quad_vertex_id = gl_VertexID % 6;
  uint src_index_a = quad_id * 2;
  uint src_index_b = quad_id * 2 + 1;

  vec3 in_pos[2];
  in_pos[0] = vertex_fetch_attribute(src_index_a, pos, vec3);
  in_pos[1] = vertex_fetch_attribute(src_index_b, pos, vec3);
  vec4 out_pos[2];
  out_pos[0] = vertex_main(model_mat, in_pos[0]);
  out_pos[1] = vertex_main(model_mat, in_pos[1]);

  /* Clip line against near plane to avoid deformed lines. */
  vec4 pos0 = out_pos[0];
  vec4 pos1 = out_pos[1];
  const vec2 pz_ndc = vec2(pos0.z / pos0.w, pos1.z / pos1.w);
  const bvec2 clipped = lessThan(pz_ndc, vec2(-1.0));
  if (all(clipped)) {
    /* Totally clipped. */
    gl_Position = vec4(0.0);
    return;
  }

  const vec4 pos01 = pos0 - pos1;
  const float ofs = abs((pz_ndc.y + 1.0) / (pz_ndc.x - pz_ndc.y));
  if (clipped.y) {
    pos1 += pos01 * ofs;
  }
  else if (clipped.x) {
    pos0 -= pos01 * (1.0 - ofs);
  }

  vec2 screen_space_pos[2];
  screen_space_pos[0] = pos0.xy / pos0.w;
  screen_space_pos[1] = pos1.xy / pos1.w;

  float half_size = max(wire_width / 2.0, 0.5);
  if (do_smooth_wire) {
    /* Add 1px for AA */
    half_size += 0.5;
  }

  const vec2 line = (screen_space_pos[0] - screen_space_pos[1]) * sizeViewport.xy;
  const vec2 line_norm = normalize(vec2(line[1], -line[0]));
  vec2 edge_ofs = (half_size * line_norm) * sizeViewportInv;

  if (quad_vertex_id == 0) {
    do_vertex(pos0, edge_ofs, half_size, 1.0);
  }
  else if (quad_vertex_id == 1 || quad_vertex_id == 3) {
    do_vertex(pos0, edge_ofs, -half_size, -1.0);
  }
  else if (quad_vertex_id == 2 || quad_vertex_id == 5) {
    do_vertex(pos1, edge_ofs, half_size, 1.0);
  }
  else if (quad_vertex_id == 4) {
    do_vertex(pos1, edge_ofs, -half_size, -1.0);
  }
}
