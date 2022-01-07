/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bmesh
 */

/**
 * \brief BM_mesh_decimate
 * \param bm: The mesh
 * \param factor: face count multiplier [0 - 1]
 * \param vweights: Optional array of vertex  aligned weights [0 - 1],
 *        a vertex group is the usual source for this.
 * \param symmetry_axis: Axis of symmetry, -1 to disable mirror decimate.
 * \param symmetry_eps: Threshold when matching mirror verts.
 *
 * \note The caller is responsible for recalculating face and vertex normals.
 * - Vertex normals are maintained while decimating,
 *   although they won't necessarily match the final recalculated normals.
 * - Face normals are not maintained at all.
 */
void BM_mesh_decimate_collapse(BMesh *bm,
                               float factor,
                               float *vweights,
                               float vweight_factor,
                               bool do_triangulate,
                               int symmetry_axis,
                               float symmetry_eps);

/**
 * \param tag_only: so we can call this from an operator */
void BM_mesh_decimate_unsubdivide_ex(BMesh *bm, int iterations, bool tag_only);
void BM_mesh_decimate_unsubdivide(BMesh *bm, int iterations);

void BM_mesh_decimate_dissolve_ex(BMesh *bm,
                                  float angle_limit,
                                  bool do_dissolve_boundaries,
                                  BMO_Delimit delimit,
                                  BMVert **vinput_arr,
                                  int vinput_len,
                                  BMEdge **einput_arr,
                                  int einput_len,
                                  short oflag_out);
void BM_mesh_decimate_dissolve(BMesh *bm,
                               float angle_limit,
                               bool do_dissolve_boundaries,
                               const BMO_Delimit delimit);
