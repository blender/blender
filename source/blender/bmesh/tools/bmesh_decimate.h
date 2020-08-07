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

void BM_mesh_decimate_collapse(BMesh *bm,
                               const float factor,
                               float *vweights,
                               float vweight_factor,
                               const bool do_triangulate,
                               const int symmetry_axis,
                               const float symmetry_eps);

void BM_mesh_decimate_unsubdivide_ex(BMesh *bm, const int iterations, const bool tag_only);
void BM_mesh_decimate_unsubdivide(BMesh *bm, const int iterations);

void BM_mesh_decimate_dissolve_ex(BMesh *bm,
                                  const float angle_limit,
                                  const bool do_dissolve_boundaries,
                                  BMO_Delimit delimit,
                                  BMVert **vinput_arr,
                                  const int vinput_len,
                                  BMEdge **einput_arr,
                                  const int einput_len,
                                  const short oflag_out);
void BM_mesh_decimate_dissolve(BMesh *bm,
                               const float angle_limit,
                               const bool do_dissolve_boundaries,
                               const BMO_Delimit delimit);
