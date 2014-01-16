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

struct BMAllocTemplate;

BMFace *BM_face_create_quad_tri(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
                                const BMFace *f_example, const eBMCreateFlag create_flag);

void BM_face_copy_shared(BMesh *bm, BMFace *f,
                         BMElemFilterFunc filter_fn, void *user_data);

BMFace *BM_face_create_ngon(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **edges, const int len,
                            const BMFace *f_example, const eBMCreateFlag create_flag);
BMFace *BM_face_create_ngon_verts(BMesh *bm, BMVert **vert_arr, const int len,
                                  const BMFace *f_example, const eBMCreateFlag create_flag,
                                  const bool calc_winding, const bool create_edges);

BMFace *BM_face_create_ngon_vcloud(BMesh *bm, BMVert **vert_arr, int len,
                                   const BMFace *f_example, const eBMCreateFlag create_flag);

void BM_elem_attrs_copy_ex(BMesh *bm_src, BMesh *bm_dst, const void *ele_src_v, void *ele_dst_v,
                           const char hflag_mask);
void BM_elem_attrs_copy(BMesh *bm_src, BMesh *bm_dst, const void *ele_src_v, void *ele_dst_v);
void BM_elem_select_copy(BMesh *bm_dst, BMesh *bm_src, void *ele_dst_v, const void *ele_src_v);

void   BM_mesh_copy_init_customdata(BMesh *bm_dst, BMesh *bm_src, const struct BMAllocTemplate *allocsize);
BMesh *BM_mesh_copy(BMesh *bm_old);

char  BM_face_flag_from_mflag(const char  mflag);
char  BM_edge_flag_from_mflag(const short mflag);
char  BM_vert_flag_from_mflag(const char  mflag);
char  BM_face_flag_to_mflag(BMFace *f);
short BM_edge_flag_to_mflag(BMEdge *e);
char  BM_vert_flag_to_mflag(BMVert *v);

#endif /* __BMESH_CONSTRUCT_H__ */
