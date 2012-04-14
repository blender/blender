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

#ifndef __BMESH_CORE_H__
#define __BMESH_CORE_H__

/** \file blender/bmesh/intern/bmesh_core.h
 *  \ingroup bmesh
 */

BMFace *BM_face_copy(BMesh *bm, BMFace *f, const short copyverts, const short copyedges);

BMVert *BM_vert_create(BMesh *bm, const float co[3], const BMVert *example);
BMEdge *BM_edge_create(BMesh *bm, BMVert *v1, BMVert *v2, const BMEdge *example, int nodouble);
BMFace *BM_face_create(BMesh *bm, BMVert **verts, BMEdge **edges, const int len, int nodouble);

void    BM_face_edges_kill(BMesh *bm, BMFace *f);
void    BM_face_verts_kill(BMesh *bm, BMFace *f);

void    BM_face_kill(BMesh *bm, BMFace *f);
void    BM_edge_kill(BMesh *bm, BMEdge *e);
void    BM_vert_kill(BMesh *bm, BMVert *v);

int     bmesh_edge_separate(BMesh *bm, BMEdge *e, BMLoop *l_sep);
int     BM_edge_splice(BMesh *bm, BMEdge *e, BMEdge *etarget);
int     BM_vert_splice(BMesh *bm, BMVert *v, BMVert *vtarget);

int     bmesh_vert_separate(BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len);

int     bmesh_loop_reverse(BMesh *bm, BMFace *f);

BMFace *BM_faces_join(BMesh *bm, BMFace **faces, int totface, const short do_del);
int     BM_vert_separate(BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len,
                         BMEdge **e_in, int e_in_len);

/* EULER API - For modifying structure */
BMFace *bmesh_sfme(BMesh *bm, BMFace *f, BMVert *v1,
                          BMVert *v2, BMLoop **r_l,
#ifdef USE_BMESH_HOLES
                          ListBase *holes,
#endif
                          BMEdge *example,
                          const short nodouble
                          );

BMVert *bmesh_semv(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **r_e);
BMEdge *bmesh_jekv(BMesh *bm, BMEdge *ke, BMVert *kv, const short check_edge_splice);
BMFace *bmesh_jfke(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e);
BMVert *bmesh_urmv(BMesh *bm, BMFace *sf, BMVert *sv);
BMVert *bmesh_urmv_loop(BMesh *bm, BMLoop *sl);

#endif /* __BMESH_CORE_H__ */
