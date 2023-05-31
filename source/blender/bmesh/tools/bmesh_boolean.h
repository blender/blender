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

bool BM_mesh_boolean(BMesh *bm,
                     struct BMLoop *(*looptris)[3],
                     int looptris_tot,
                     int (*test_fn)(BMFace *f, void *user_data),
                     void *user_data,
                     int nshapes,
                     bool use_self,
                     bool keep_hidden,
                     bool hole_tolerant,
                     int boolean_mode);

/**
 * Perform a Knife Intersection operation on the mesh `bm`.
 * There are either one or two operands, the same as described above for #BM_mesh_boolean().
 *
 * \param use_separate_all: When true, each edge that is created from the intersection should
 * be used to separate all its incident faces. TODO: implement that.
 *
 * TODO: need to ensure that "selected/non-selected" flag of original faces gets propagated
 * to the intersection result faces.
 */
bool BM_mesh_boolean_knife(BMesh *bm,
                           struct BMLoop *(*looptris)[3],
                           int looptris_tot,
                           int (*test_fn)(BMFace *f, void *user_data),
                           void *user_data,
                           int nshapes,
                           bool use_self,
                           bool use_separate_all,
                           bool hole_tolerant,
                           bool keep_hidden);

#ifdef __cplusplus
}
#endif
