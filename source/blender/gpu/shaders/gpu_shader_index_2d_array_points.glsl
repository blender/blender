/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 *  Constructs a simple 2D array index buffer, with 'ncurves' rows and 'elements_per_curve'
 *  columns. Each row contains 'elements_per_curve-1' indexes and a restart index.
 *  The index buffer can then be used to draw either 'ncurves' lines with 'elements_per_curve-1'
 *  vertexes each, or 'ncurves' triangle strips with 'elements_per_curve-3' triangles each.
 */
void main()
{
  ivec3 gid = ivec3(gl_GlobalInvocationID);
  ivec3 nthreads = ivec3(gl_NumWorkGroups) * ivec3(gl_WorkGroupSize);
  for (int y = gid.y + gid.z * nthreads.y; y < ncurves; y += nthreads.y * nthreads.z) {
    for (int x = gid.x; x < elements_per_curve; x += nthreads.x) {
      int store_index = x + y * elements_per_curve;
      out_indices[store_index] = (x + 1 < elements_per_curve) ?
                                     uint(x + y * (elements_per_curve - 1)) :
                                     0xFFFFFFFFu;
    }
  }
}
