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
