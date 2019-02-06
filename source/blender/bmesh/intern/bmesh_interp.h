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

#ifndef __BMESH_INTERP_H__
#define __BMESH_INTERP_H__

/** \file \ingroup bmesh
 */

struct LinkNode;
struct MemArena;

void  BM_loop_interp_multires_ex(
        BMesh *bm, BMLoop *l_dst, const BMFace *f_src,
        const float f_dst_center[3], const float f_src_center[3], const int cd_loop_mdisp_offset);
void  BM_loop_interp_multires(
        BMesh *bm, BMLoop *l_dst, const BMFace *f_src);

void BM_face_interp_multires_ex(
        BMesh *UNUSED(bm), BMFace *f_dst, const BMFace *f_src,
        const float f_dst_center[3], const float f_src_center[3], const int cd_loop_mdisp_offset);
void BM_face_interp_multires(BMesh *bm, BMFace *f_dst, const BMFace *f_src);

void  BM_vert_interp_from_face(BMesh *bm, BMVert *v_dst, const BMFace *f_src);

void  BM_data_interp_from_verts(BMesh *bm, const BMVert *v_src_1, const BMVert *v_src_2, BMVert *v_dst, const float fac);
void  BM_data_interp_from_edges(BMesh *bm, const BMEdge *e_src_1, const BMEdge *e_src_2, BMEdge *e_dst, const float fac);
void  BM_data_interp_face_vert_edge(BMesh *bm, const BMVert *v_src_1, const BMVert *v_src_2, BMVert *v, BMEdge *e, const float fac);
void  BM_data_layer_add(BMesh *bm, CustomData *data, int type);
void  BM_data_layer_add_named(BMesh *bm, CustomData *data, int type, const char *name);
void  BM_data_layer_free(BMesh *bm, CustomData *data, int type);
void  BM_data_layer_free_n(BMesh *bm, CustomData *data, int type, int n);
void  BM_data_layer_copy(BMesh *bm, CustomData *data, int type, int src_n, int dst_n);

float BM_elem_float_data_get(CustomData *cd, void *element, int type);
void  BM_elem_float_data_set(CustomData *cd, void *element, int type, const float val);

void BM_face_interp_from_face_ex(
        BMesh *bm, BMFace *f_dst, const BMFace *f_src, const bool do_vertex,
        const void **blocks, const void **blocks_v,
        float (*cos_2d)[2], float axis_mat[3][3]);
void  BM_face_interp_from_face(
        BMesh *bm, BMFace *f_dst, const BMFace *f_src,
        const bool do_vertex);
void  BM_loop_interp_from_face(
        BMesh *bm, BMLoop *l_dst, const BMFace *f_src,
        const bool do_vertex, const bool do_multires);

void  BM_face_multires_bounds_smooth(BMesh *bm, BMFace *f);

struct LinkNode *BM_vert_loop_groups_data_layer_create(
        BMesh *bm, BMVert *v, const int layer_n,
        const float *loop_weights, struct MemArena *arena);
void BM_vert_loop_groups_data_layer_merge(
        BMesh *bm, struct LinkNode *groups, const int layer_n);
void BM_vert_loop_groups_data_layer_merge_weights(
        BMesh *bm, struct LinkNode *groups, const int layer_n,
        const float *loop_weights);

#endif /* __BMESH_INTERP_H__ */
