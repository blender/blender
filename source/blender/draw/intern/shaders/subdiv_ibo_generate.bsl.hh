/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Generate triangle indices from subdivision quads.
 * Create index buffer for lines and loose lines.
 */

#pragma once

#include "subdiv_common.bsl.hh"

namespace subdiv {

struct LinesResources {
  [[resource_table]] const SubdivResources srt;

  [[storage(LINES_INPUT_EDGE_DRAW_FLAG_BUF_SLOT, read)]] int (&input_edge_draw_flag)[];
  [[storage(LINES_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read)]] uint (&extra_coarse_face_data)[];
  [[storage(LINES_OUTPUT_LINES_BUF_SLOT, write)]] uint (&output_lines)[];

  bool is_face_hidden(uint coarse_quad_index)
  {
    return (extra_coarse_face_data[coarse_quad_index] & srt.shader_data.coarse_face_hidden_mask) !=
           0;
  }
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void lines_main([[resource_table]] const SubdivResources &srt,
                [[resource_table]] const PolygonOffsetBase &poly_ofs,
                [[resource_table]] LinesResources &res,
                [[global_invocation_id]] const uint3 global_id,
                [[num_work_groups]] const uint3 num_work_groups)
{
  uint index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  uint quad_index = index;
  /* We execute for each quad, so the start index of the loop is quad_index * 4. */
  uint start_loop_index = index * 4;
  /* We execute for each quad, so the start index of the line is quad_index * 8 (with 2 vertices
   * per line). */
  uint start_line_index = index * 8;

  uint coarse_quad_index = poly_ofs.coarse_face_index_from_subdiv_quad_index(
      quad_index, uint(srt.shader_data.coarse_face_count));

  for (uint i = 0; i < 4; i++) {
    uint line_offset = start_line_index + i * 2u;
    uint vertex_index = start_loop_index + i;

    if ((srt.shader_data.use_hide && res.is_face_hidden(coarse_quad_index)) ||
        (res.input_edge_draw_flag[vertex_index] == 0))
    {
      res.output_lines[line_offset + 0] = 0xffffffff;
      res.output_lines[line_offset + 1] = 0xffffffff;
    }
    else {
      /* Mod 4 so we loop back at the first vertex on the last loop index (3). */
      uint next_vertex_index = start_loop_index + (i + 1) % 4;

      res.output_lines[line_offset + 0] = vertex_index;
      res.output_lines[line_offset + 1] = next_vertex_index;
    }
  }
}

struct LinesLooseResources {
  [[storage(LINES_LINES_LOOSE_FLAGS, read)]] uint (&lines_loose_flags)[];
  [[storage(LINES_OUTPUT_LINES_BUF_SLOT, write)]] uint (&output_lines)[];
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void lines_loose_main([[resource_table]] const SubdivResources &srt,
                      [[resource_table]] LinesLooseResources &res,
                      [[global_invocation_id]] const uint3 global_id,
                      [[num_work_groups]] const uint3 num_work_groups)
{
  uint index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  /* In the loose lines case, we execute for each line, with two vertices per line. */
  uint line_offset = srt.shader_data.edge_loose_offset + index * 2;
  uint loop_index = srt.shader_data.num_subdiv_loops + index * 2;

  if (res.lines_loose_flags[index] != 0) {
    /* Line is hidden. */
    res.output_lines[line_offset] = 0xffffffff;
    res.output_lines[line_offset + 1] = 0xffffffff;
  }
  else {
    res.output_lines[line_offset] = loop_index;
    res.output_lines[line_offset + 1] = loop_index + 1;
  }
}

struct TriangleConstants {
  [[compilation_constant]] const bool is_multi_material;
};

struct TriangleResources {
  [[resource_table]] TriangleConstants constants;
  [[resource_table]] const SubdivResources srt;

  [[storage(TRIS_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read)]] uint (&extra_coarse_face_data)[];
  [[storage(TRIS_OUTPUT_TRIS_BUF_SLOT, write)]] uint (&output_tris)[];

  [[storage(TRIS_FACE_MAT_OFFSET, read), condition(is_multi_material)]] uint (&face_mat_offset)[];

  bool is_face_hidden(uint coarse_quad_index) const
  {
    return (extra_coarse_face_data[coarse_quad_index] & srt.shader_data.coarse_face_hidden_mask) !=
           0;
  }
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void tris_main([[resource_table]] const SubdivResources &srt,
               [[resource_table]] const PolygonOffsetBase &poly_ofs,
               [[resource_table]] TriangleResources &res,
               [[resource_table]] TriangleConstants &constants,
               [[global_invocation_id]] const uint3 global_id,
               [[num_work_groups]] const uint3 num_work_groups)
{
  uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  uint loop_index = quad_index * 4;
  uint coarse_quad_index = poly_ofs.coarse_face_index_from_subdiv_quad_index(
      quad_index, uint(srt.shader_data.coarse_face_count));

  uint triangle_loop_index = quad_index * 6;

  if (constants.is_multi_material) [[static_branch]] {
    triangle_loop_index = (quad_index + res.face_mat_offset[coarse_quad_index]) * 6;
  }

  if (srt.shader_data.use_hide && res.is_face_hidden(coarse_quad_index)) {
    /* Replace with restart indices. */
    res.output_tris[triangle_loop_index + 0] = 0xFFFFFFFFu;
    res.output_tris[triangle_loop_index + 1] = 0xFFFFFFFFu;
    res.output_tris[triangle_loop_index + 2] = 0xFFFFFFFFu;
    res.output_tris[triangle_loop_index + 3] = 0xFFFFFFFFu;
    res.output_tris[triangle_loop_index + 4] = 0xFFFFFFFFu;
    res.output_tris[triangle_loop_index + 5] = 0xFFFFFFFFu;
  }
  else {
    res.output_tris[triangle_loop_index + 0] = loop_index + 0;
    res.output_tris[triangle_loop_index + 1] = loop_index + 1;
    res.output_tris[triangle_loop_index + 2] = loop_index + 2;
    res.output_tris[triangle_loop_index + 3] = loop_index + 0;
    res.output_tris[triangle_loop_index + 4] = loop_index + 2;
    res.output_tris[triangle_loop_index + 5] = loop_index + 3;
  }
}

PipelineCompute lines(lines_main);
PipelineCompute lines_loose(lines_loose_main);
PipelineCompute tris_single_material(tris_main, TriangleConstants{.is_multi_material = false});
PipelineCompute tris_multiple_materials(tris_main, TriangleConstants{.is_multi_material = true});

}  // namespace subdiv
