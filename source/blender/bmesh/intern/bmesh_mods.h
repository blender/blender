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

bool BM_vert_dissolve(BMesh *bm, BMVert *v);

bool BM_disk_dissolve(BMesh *bm, BMVert *v);

BMFace *BM_faces_join_pair(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e, const bool do_del);

BMFace *BM_face_split(BMesh *bm, BMFace *f,
                      BMLoop *l_a, BMLoop *l_b,
                      BMLoop **r_l,
                      BMEdge *example, const bool no_double);

BMFace *BM_face_split_n(BMesh *bm, BMFace *f,
                        BMLoop *l_a, BMLoop *l_b,
                        float cos[][3], int n,
                        BMLoop **r_l, BMEdge *example);

BMEdge *BM_vert_collapse_faces(BMesh *bm, BMEdge *e_kill, BMVert *v_kill, float fac,
                               const bool join_faces, const bool kill_degenerate_faces);
BMEdge *BM_vert_collapse_edge(BMesh *bm, BMEdge *e_kill, BMVert *v_kill,
                              const bool kill_degenerate_faces);


BMVert *BM_edge_split(BMesh *bm, BMEdge *e, BMVert *v, BMEdge **r_e, float percent);

BMVert *BM_edge_split_n(BMesh *bm, BMEdge *e, int numcuts, BMVert **r_varr);

bool    BM_face_validate(BMFace *face, FILE *err);

void    BM_edge_calc_rotate(BMEdge *e, const bool ccw,
                            BMLoop **r_l1, BMLoop **r_l2);
bool    BM_edge_rotate_check(BMEdge *e);
bool    BM_edge_rotate_check_degenerate(BMEdge *e,
                                        BMLoop *l1, BMLoop *l2);
bool    BM_edge_rotate_check_beauty(BMEdge *e,
                                    BMLoop *l1, BMLoop *l2);
BMEdge *BM_edge_rotate(BMesh *bm, BMEdge *e, const bool ccw, const short check_flag);

/* flags for BM_edge_rotate */
enum {
	BM_EDGEROT_CHECK_EXISTS     = (1 << 0), /* disallow to rotate when the new edge matches an existing one */
	BM_EDGEROT_CHECK_SPLICE     = (1 << 1), /* overrides existing check, if the edge already, rotate and merge them */
	BM_EDGEROT_CHECK_DEGENERATE = (1 << 2), /* disallow creating bow-tie, concave or zero area faces */
	BM_EDGEROT_CHECK_BEAUTY     = (1 << 3)  /* disallow to rotate into ugly topology */
};


BMVert *BM_face_vert_separate(BMesh *bm, BMFace *sf, BMVert *sv);
BMVert *BM_face_loop_separate(BMesh *bm, BMLoop *sl);

#endif /* __BMESH_MODS_H__ */
