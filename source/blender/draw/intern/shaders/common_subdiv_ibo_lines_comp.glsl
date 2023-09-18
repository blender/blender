/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* To be compiled with common_subdiv_lib.glsl */

layout(std430, binding = 1) readonly buffer inputEdgeDrawFlag
{
  int input_edge_draw_flag[];
};

layout(std430, binding = 2) readonly restrict buffer extraCoarseFaceData
{
  uint extra_coarse_face_data[];
};

layout(std430, binding = 3) writeonly buffer outputLinesIndices
{
  uint output_lines[];
};

layout(std430, binding = 4) readonly buffer LinesLooseFlags
{
  uint lines_loose_flags[];
};

#ifndef LINES_LOOSE

bool is_face_hidden(uint coarse_quad_index)
{
  return (extra_coarse_face_data[coarse_quad_index] & coarse_face_hidden_mask) != 0;
}

void emit_line(uint line_offset, uint quad_index, uint start_loop_index, uint corner_index)
{
  uint vertex_index = start_loop_index + corner_index;

  uint coarse_quad_index = coarse_face_index_from_subdiv_quad_index(quad_index, coarse_face_count);

  if (use_hide && is_face_hidden(coarse_quad_index) || (input_edge_draw_flag[vertex_index] == 0)) {
    output_lines[line_offset + 0] = 0xffffffff;
    output_lines[line_offset + 1] = 0xffffffff;
  }
  else {
    /* Mod 4 so we loop back at the first vertex on the last loop index (3). */
    uint next_vertex_index = start_loop_index + (corner_index + 1) % 4;

    output_lines[line_offset + 0] = vertex_index;
    output_lines[line_offset + 1] = next_vertex_index;
  }
}
#endif

void main()
{
  uint index = get_global_invocation_index();
  if (index >= total_dispatch_size) {
    return;
  }

#ifdef LINES_LOOSE
  /* In the loose lines case, we execute for each line, with two vertices per line. */
  uint line_offset = edge_loose_offset + index * 2;
  uint loop_index = num_subdiv_loops + index * 2;

  if (lines_loose_flags[index] != 0) {
    /* Line is hidden. */
    output_lines[line_offset] = 0xffffffff;
    output_lines[line_offset + 1] = 0xffffffff;
  }
  else {
    output_lines[line_offset] = loop_index;
    output_lines[line_offset + 1] = loop_index + 1;
  }

#else
  /* We execute for each quad, so the start index of the loop is quad_index * 4. */
  uint start_loop_index = index * 4;
  /* We execute for each quad, so the start index of the line is quad_index * 8 (with 2 vertices
   * per line). */
  uint start_line_index = index * 8;

  for (int i = 0; i < 4; i++) {
    emit_line(start_line_index + i * 2, index, start_loop_index, i);
  }
#endif
}
