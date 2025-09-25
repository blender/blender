/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_index_infos.hh"

COMPUTE_SHADER_CREATE_INFO(gpu_shader_index_2d_array_tris)

/**
 *  Constructs a 2D array index buffer, with 'ncurves' rows and 'elements_per_curve*6' columns.
 *  The index buffer can be used to draw 'ncurves' triangle strips with 'elements_per_curve*2'
 *  triangles each, using GL_TRIANGLES primitives. Intended to be used if GL_TRIANGLE_STRIP
 *  primitives can't be used for some reason.
 */
void main()
{
  int3 gid = int3(gl_GlobalInvocationID);
  int3 nthreads = int3(gl_NumWorkGroups * uint3(gl_WorkGroupSize));
  for (int y = gid.y + gid.z * nthreads.y; y < ncurves; y += nthreads.y * nthreads.z) {
    for (int x = gid.x; x < elements_per_curve; x += nthreads.x) {
      int store_index = (x + y * elements_per_curve) * 6;
      uint t = x * 2 + y * (elements_per_curve * 2 + 2);
      out_indices[store_index + 0] = t;
      out_indices[store_index + 1] = t + 1u;
      out_indices[store_index + 2] = t + 2u;
      out_indices[store_index + 3] = t + 1u;
      out_indices[store_index + 4] = t + 3u;
      out_indices[store_index + 5] = t + 2u;
    }
  }
}
