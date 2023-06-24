/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
