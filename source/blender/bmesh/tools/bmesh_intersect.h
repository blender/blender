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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Intersect tessellated faces
 * leaving the resulting edges tagged.
 *
 * \param test_fn: Return value: -1: skip, 0: tree_a, 1: tree_b (use_self == false)
 * \param boolean_mode: -1: no-boolean, 0: intersection... see #BMESH_ISECT_BOOLEAN_ISECT.
 * \return true if the mesh is changed (intersections cut or faces removed from boolean).
 */
bool BM_mesh_intersect(BMesh *bm,
                       struct BMLoop *(*looptris)[3],
                       int looptris_tot,
                       int (*test_fn)(BMFace *f, void *user_data),
                       void *user_data,
                       bool use_self,
                       bool use_separate,
                       bool use_dissolve,
                       bool use_island_connect,
                       bool use_partial_connect,
                       bool use_edge_tag,
                       int boolean_mode,
                       float eps);

enum {
  BMESH_ISECT_BOOLEAN_NONE = -1,
  /* aligned with BooleanModifierOp */
  BMESH_ISECT_BOOLEAN_ISECT = 0,
  BMESH_ISECT_BOOLEAN_UNION = 1,
  BMESH_ISECT_BOOLEAN_DIFFERENCE = 2,
};

#ifdef __cplusplus
}
#endif
