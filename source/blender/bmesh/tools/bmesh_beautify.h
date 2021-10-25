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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_BEAUTIFY_H__
#define __BMESH_BEAUTIFY_H__

/** \file blender/bmesh/tools/bmesh_beautify.h
 *  \ingroup bmesh
 */

enum {
	VERT_RESTRICT_TAG = (1 << 0),
};

void BM_mesh_beautify_fill(
        BMesh *bm, BMEdge **edge_array, const int edge_array_len,
        const short flag, const short method,
        const short oflag_edge, const short oflag_face);

float BM_verts_calc_rotate_beauty(
        const BMVert *v1, const BMVert *v2,
        const BMVert *v3, const BMVert *v4,
        const short flag, const short method);

#endif /* __BMESH_BEAUTIFY_H__ */
