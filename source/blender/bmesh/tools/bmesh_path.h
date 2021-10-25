/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_PATH_H__
#define __BMESH_PATH_H__

/** \file blender/bmesh/tools/bmesh_path.h
 *  \ingroup bmesh
 */

struct BMCalcPathParams {
	uint use_topology_distance : 1;
	uint use_step_face : 1;
};

struct LinkNode *BM_mesh_calc_path_vert(
        BMesh *bm, BMVert *v_src, BMVert *v_dst, const struct BMCalcPathParams *params,
        bool (*filter_fn)(BMVert *, void *), void *user_data)
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3, 5);

struct LinkNode *BM_mesh_calc_path_edge(
        BMesh *bm, BMEdge *e_src, BMEdge *e_dst, const struct BMCalcPathParams *params,
        bool (*filter_fn)(BMEdge *, void *), void *user_data)
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3, 5);

struct LinkNode *BM_mesh_calc_path_face(
        BMesh *bm, BMFace *f_src, BMFace *f_dst, const struct BMCalcPathParams *params,
        bool (*filter_fn)(BMFace *, void *), void *user_data)
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3, 5);

#endif /* __BMESH_PATH_H__ */
