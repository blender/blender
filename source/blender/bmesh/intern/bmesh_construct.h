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

#ifndef __BMESH_CONSTRUCT_H__
#define __BMESH_CONSTRUCT_H__

/** \file blender/bmesh/intern/bmesh_construct.h
 *  \ingroup bmesh
 */

BMFace *BM_face_create_quad_tri_v(BMesh *bm,
                                  BMVert **verts, int len,
                                  const BMFace *example, const int nodouble);

BMFace *BM_face_create_quad_tri(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
                                const BMFace *example, const int nodouble);

void BM_face_copy_shared(BMesh *bm, BMFace *f);

BMFace *BM_face_create_ngon(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **edges, int len, int nodouble);

BMFace *BM_face_create_ngon_vcloud(BMesh *bm, BMVert **vert_arr, int len, int nodouble);

void BMO_remove_tagged_faces(BMesh *bm, const short oflag);
void BMO_remove_tagged_edges(BMesh *bm, const short oflag);
void BMO_remove_tagged_verts(BMesh *bm, const short oflag);

void BMO_remove_tagged_context(BMesh *bm, const short oflag, const int type);

void BM_elem_attrs_copy(BMesh *source_mesh, BMesh *target_mesh, const void *source, void *target);

BMesh *BM_mesh_copy(BMesh *bm_old);

char  BM_face_flag_from_mflag(const char  mflag);
char  BM_edge_flag_from_mflag(const short mflag);
char  BM_vert_flag_from_mflag(const char  mflag);
char  BM_face_flag_to_mflag(BMFace *f);
short BM_edge_flag_to_mflag(BMEdge *e);
char  BM_vert_flag_to_mflag(BMVert *v);

#endif /* __BMESH_CONSTRUCT_H__ */
