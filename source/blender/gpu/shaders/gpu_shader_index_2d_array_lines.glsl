/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_index_infos.hh"

COMPUTE_SHADER_CREATE_INFO(gpu_shader_index_2d_array_lines)

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
void main()
{
  int3 gid = int3(gl_GlobalInvocationID);
  int3 nthreads = int3(gl_NumWorkGroups * uint3(gl_WorkGroupSize));
  for (int y = gid.y + gid.z * nthreads.y; y < ncurves; y += nthreads.y * nthreads.z) {
    for (int x = gid.x; x < elements_per_curve; x += nthreads.x) {
      int store_index = (x + y * elements_per_curve) * 2;
      uint t = uint(x + y * (elements_per_curve + 1));
      out_indices[store_index] = t;
      out_indices[store_index + 1] = t + 1u;
    }
  }
}
