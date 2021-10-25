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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_PATH_REGION_H__
#define __BMESH_PATH_REGION_H__

/** \file blender/bmesh/tools/bmesh_path_region.h
 *  \ingroup bmesh
 */

struct LinkNode *BM_mesh_calc_path_region_vert(
        BMesh *bm, BMElem *ele_src, BMElem *ele_dst,
        bool (*test_fn)(BMVert *, void *user_data), void *user_data)
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);

struct LinkNode *BM_mesh_calc_path_region_edge(
        BMesh *bm, BMElem *ele_src, BMElem *ele_dst,
        bool (*test_fn)(BMEdge *, void *user_data), void *user_data)
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);

struct LinkNode *BM_mesh_calc_path_region_face(
        BMesh *bm, BMElem *ele_src, BMElem *ele_dst,
        bool (*test_fn)(BMFace *, void *user_data), void *user_data)
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);

#endif /* __BMESH_PATH_REGION_H__ */
