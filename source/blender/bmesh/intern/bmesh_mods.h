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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_MODS_H__
#define __BMESH_MODS_H__

/** \file blender/bmesh/intern/bmesh_mods.h
 *  \ingroup bmesh
 */

#include <stdio.h>

int BM_vert_dissolve(BMesh *bm, BMVert *v);

int BM_disk_dissolve(BMesh *bm, BMVert *v);

BMFace *BM_faces_join_pair(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e);

BMEdge *BM_verts_connect(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **r_f);

BMFace *BM_face_split(BMesh *bm, BMFace *f,
                      BMVert *v1, BMVert *v2,
                      BMLoop **r_l, BMEdge *example);

BMEdge* BM_vert_collapse_faces(BMesh *bm, BMEdge *ke, BMVert *kv, float fac,
                               const short join_faces, const short kill_degenerate_faces);
BMEdge* BM_vert_collapse_edge(BMesh *bm, BMEdge *ke, BMVert *kv,
                              const short kill_degenerate_faces);


BMVert *BM_edge_split(BMesh *bm, BMEdge *e, BMVert *v, BMEdge **r_e, float percent);

BMVert *BM_edge_split_n(BMesh *bm, BMEdge *e, int numcuts);

int     BM_face_validate(BMesh *bm, BMFace *face, FILE *err);

BMEdge *BM_edge_rotate(BMesh *bm, BMEdge *e, int ccw);

BMVert *BM_vert_rip(BMesh *bm, BMFace *sf, BMVert *sv);

#endif /* __BMESH_MODS_H__ */
