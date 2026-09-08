/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

namespace index_gen {

struct Resources {
  [[push_constant]] int elements_per_curve;
  [[push_constant]] int ncurves;
  [[storage(0, write)]] uint (&out_indices)[];
};

/**
 *  Constructs a simple 2D array index buffer, with 'ncurves' rows and 'elements_per_curve'
 *  columns. Each row contains 'elements_per_curve-1' indexes and a restart index.
 *  The index buffer can then be used to draw either 'ncurves' lines with 'elements_per_curve-1'
 *  vertexes each, or 'ncurves' triangle strips with 'elements_per_curve-3' triangles each.
 */
[[compute]] [[local_size(16, 16, 1)]] void gen_points(
    [[resource_table]] const Resources &srt,
    [[global_invocation_id]] const uint3 global_id,
    [[num_work_groups]] const uint3 num_groups)
{
  int3 gid = int3(global_id);
  int3 nthreads = int3(num_groups * uint3(gl_WorkGroupSize));
  for (int y = gid.y + gid.z * nthreads.y; y < srt.ncurves; y += nthreads.y * nthreads.z) {
    for (int x = gid.x; x < srt.elements_per_curve; x += nthreads.x) {
      int store_index = x + y * srt.elements_per_curve;
      srt.out_indices[store_index] = (x + 1 < srt.elements_per_curve) ?
                                         uint(x + y * (srt.elements_per_curve - 1)) :
                                         0xFFFFFFFFu;
    }
  }
}

/**
 *  Constructs a 2D array index buffer with 'ncurves' rows and 'elements_per_curve*2'
 *  columns. Each row contains 'elements_per_curve' pairs of indexes.
 *  e.g., for elements_per_curve=32, first two rows are
 *  0 1 1 2 2 3 ... 31 32
 *  33 34 34 35 35 36 .. 64 65
 *  The index buffer can then be used to draw 'ncurves' curves with 'elements_per_curve+1'
 *  vertexes each, using GL_LINES primitives. Intended to be used if GL_LINE_STRIP
 *  primitives can't be used for some reason.
 */
[[compute]] [[local_size(16, 16, 1)]] void gen_lines(
    [[resource_table]] const Resources &srt,
    [[global_invocation_id]] const uint3 global_id,
    [[num_work_groups]] const uint3 num_groups)
{
  int3 gid = int3(global_id);
  int3 nthreads = int3(num_groups * uint3(gl_WorkGroupSize));
  for (int y = gid.y + gid.z * nthreads.y; y < srt.ncurves; y += nthreads.y * nthreads.z) {
    for (int x = gid.x; x < srt.elements_per_curve; x += nthreads.x) {
      int store_index = (x + y * srt.elements_per_curve) * 2;
      uint t = uint(x + y * (srt.elements_per_curve + 1));
      srt.out_indices[store_index] = t;
      srt.out_indices[store_index + 1] = t + 1u;
    }
  }
}

/**
 *  Constructs a 2D array index buffer, with 'ncurves' rows and 'elements_per_curve*6' columns.
 *  The index buffer can be used to draw 'ncurves' triangle strips with 'elements_per_curve*2'
 *  triangles each, using GL_TRIANGLES primitives. Intended to be used if GL_TRIANGLE_STRIP
 *  primitives can't be used for some reason.
 */
[[compute]] [[local_size(16, 16, 1)]] void gen_triangles(
    [[resource_table]] const Resources &srt,
    [[global_invocation_id]] const uint3 global_id,
    [[num_work_groups]] const uint3 num_groups)
{
  int3 gid = int3(global_id);
  int3 nthreads = int3(num_groups * uint3(gl_WorkGroupSize));
  for (int y = gid.y + gid.z * nthreads.y; y < srt.ncurves; y += nthreads.y * nthreads.z) {
    for (int x = gid.x; x < srt.elements_per_curve; x += nthreads.x) {
      int store_index = (x + y * srt.elements_per_curve) * 6;
      uint t = x * 2 + y * (srt.elements_per_curve * 2 + 2);
      srt.out_indices[store_index + 0] = t;
      srt.out_indices[store_index + 1] = t + 1u;
      srt.out_indices[store_index + 2] = t + 2u;
      srt.out_indices[store_index + 3] = t + 1u;
      srt.out_indices[store_index + 4] = t + 3u;
      srt.out_indices[store_index + 5] = t + 2u;
    }
  }
}

}  // namespace index_gen

PipelineCompute gpu_shader_index_2d_array_points(index_gen::gen_points);
PipelineCompute gpu_shader_index_2d_array_lines(index_gen::gen_lines);
PipelineCompute gpu_shader_index_2d_array_tris(index_gen::gen_triangles);
