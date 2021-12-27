
/* To be compile with common_subdiv_lib.glsl */

layout(std430, binding = 0) readonly buffer inputEdgeOrigIndex
{
  int input_origindex[];
};

layout(std430, binding = 1) writeonly buffer outputLinesIndices
{
  uint output_lines[];
};

#ifndef LINES_LOOSE
void emit_line(uint line_offset, uint start_loop_index, uint corner_index)
{
  uint vertex_index = start_loop_index + corner_index;

  if (input_origindex[vertex_index] == ORIGINDEX_NONE && optimal_display) {
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
  output_lines[line_offset] = loop_index;
  output_lines[line_offset + 1] = loop_index + 1;
#else
  /* We execute for each quad, so the start index of the loop is quad_index * 4. */
  uint start_loop_index = index * 4;
  /* We execute for each quad, so the start index of the line is quad_index * 8 (with 2 vertices
   * per line). */
  uint start_line_index = index * 8;

  for (int i = 0; i < 4; i++) {
    emit_line(start_line_index + i * 2, start_loop_index, i);
  }
#endif
}
