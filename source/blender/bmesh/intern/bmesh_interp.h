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
 * Contributor(s): Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_INTERP_H__
#define __BMESH_INTERP_H__

/** \file blender/bmesh/intern/bmesh_interp.h
 *  \ingroup bmesh
 */

void  BM_loop_interp_multires(BMesh *bm, BMLoop *target, BMFace *source);
void  BM_vert_interp_from_face(BMesh *bm, BMVert *v, BMFace *source);

void  BM_data_interp_from_verts(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, const float fac);
void  BM_data_interp_from_edges(BMesh *bm, BMEdge *e1, BMEdge *e2, BMEdge *e, const float fac);
void  BM_data_interp_face_vert_edge(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, BMEdge *e1, const float fac);
void  BM_data_layer_add(BMesh *em, CustomData *data, int type);
void  BM_data_layer_add_named(BMesh *bm, CustomData *data, int type, const char *name);
void  BM_data_layer_free(BMesh *em, CustomData *data, int type);
void  BM_data_layer_free_n(BMesh *bm, CustomData *data, int type, int n);
void  BM_data_layer_copy(BMesh *bm, CustomData *data, int type, int src_n, int dst_n);

float BM_elem_float_data_get(CustomData *cd, void *element, int type);
void  BM_elem_float_data_set(CustomData *cd, void *element, int type, const float val);

void BM_face_interp_from_face_ex(BMesh *bm, BMFace *target, BMFace *source, const bool do_vertex,
                                 void **blocks, void **blocks_v, float (*cos_2d)[2], float axis_mat[3][3]);
void  BM_face_interp_from_face(BMesh *bm, BMFace *target, BMFace *source, const bool do_vertex);
void  BM_loop_interp_from_face(BMesh *bm, BMLoop *target, BMFace *source,
                               const bool do_vertex, const bool do_multires);

void  BM_face_multires_bounds_smooth(BMesh *bm, BMFace *f);

#endif /* __BMESH_INTERP_H__ */
