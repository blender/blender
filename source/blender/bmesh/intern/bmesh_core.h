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

#pragma once

/** \file
 * \ingroup bmesh
 */

BMFace *BM_face_copy(
    BMesh *bm_dst, BMesh *bm_src, BMFace *f, const bool copy_verts, const bool copy_edges);

typedef enum eBMCreateFlag {
  BM_CREATE_NOP = 0,
  /* faces and edges only */
  BM_CREATE_NO_DOUBLE = (1 << 1),
  /* Skip CustomData - for all element types data,
   * use if we immediately write customdata into the element so this skips copying from 'example'
   * args or setting defaults, speeds up conversion when data is converted all at once. */
  BM_CREATE_SKIP_CD = (1 << 2),
} eBMCreateFlag;

BMVert *BM_vert_create(BMesh *bm,
                       const float co[3],
                       const BMVert *v_example,
                       const eBMCreateFlag create_flag);
BMEdge *BM_edge_create(
    BMesh *bm, BMVert *v1, BMVert *v2, const BMEdge *e_example, const eBMCreateFlag create_flag);
BMFace *BM_face_create(BMesh *bm,
                       BMVert **verts,
                       BMEdge **edges,
                       const int len,
                       const BMFace *f_example,
                       const eBMCreateFlag create_flag);
BMFace *BM_face_create_verts(BMesh *bm,
                             BMVert **verts,
                             const int len,
                             const BMFace *f_example,
                             const eBMCreateFlag create_flag,
                             const bool create_edges);

void BM_face_edges_kill(BMesh *bm, BMFace *f);
void BM_face_verts_kill(BMesh *bm, BMFace *f);

void BM_face_kill_loose(BMesh *bm, BMFace *f);

void BM_face_kill(BMesh *bm, BMFace *f);
void BM_edge_kill(BMesh *bm, BMEdge *e);
void BM_vert_kill(BMesh *bm, BMVert *v);

bool BM_edge_splice(BMesh *bm, BMEdge *e_dst, BMEdge *e_src);
bool BM_vert_splice(BMesh *bm, BMVert *v_dst, BMVert *v_src);
bool BM_vert_splice_check_double(BMVert *v_a, BMVert *v_b);

void bmesh_kernel_loop_reverse(BMesh *bm,
                               BMFace *f,
                               const int cd_loop_mdisp_offset,
                               const bool use_loop_mdisp_flip);

void bmesh_face_swap_data(BMFace *f_a, BMFace *f_b);

BMFace *BM_faces_join(BMesh *bm, BMFace **faces, int totface, const bool do_del);
void BM_vert_separate(BMesh *bm,
                      BMVert *v,
                      BMEdge **e_in,
                      int e_in_len,
                      const bool copy_select,
                      BMVert ***r_vout,
                      int *r_vout_len);
void BM_vert_separate_hflag(BMesh *bm,
                            BMVert *v,
                            const char hflag,
                            const bool copy_select,
                            BMVert ***r_vout,
                            int *r_vout_len);
void BM_vert_separate_tested_edges(
    BMesh *bm, BMVert *v_dst, BMVert *v_src, bool (*testfn)(BMEdge *, void *arg), void *arg);

/**
 * BMesh Kernel: For modifying structure.
 *
 * Names are on the verbose side but these are only for low-level access.
 */
void bmesh_kernel_vert_separate(
    BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len, const bool copy_select);
void bmesh_kernel_edge_separate(BMesh *bm, BMEdge *e, BMLoop *l_sep, const bool copy_select);

BMFace *bmesh_kernel_split_face_make_edge(BMesh *bm,
                                          BMFace *f,
                                          BMLoop *l1,
                                          BMLoop *l2,
                                          BMLoop **r_l,
#ifdef USE_BMESH_HOLES
                                          ListBase *holes,
#endif
                                          BMEdge *example,
                                          const bool no_double);

BMVert *bmesh_kernel_split_edge_make_vert(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **r_e);
BMEdge *bmesh_kernel_join_edge_kill_vert(BMesh *bm,
                                         BMEdge *e_kill,
                                         BMVert *v_kill,
                                         const bool do_del,
                                         const bool check_edge_splice,
                                         const bool kill_degenerate_faces);
BMVert *bmesh_kernel_join_vert_kill_edge(BMesh *bm,
                                         BMEdge *e_kill,
                                         BMVert *v_kill,
                                         const bool do_del,
                                         const bool check_edge_double,
                                         const bool kill_degenerate_faces);
BMFace *bmesh_kernel_join_face_kill_edge(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e);

BMVert *bmesh_kernel_unglue_region_make_vert(BMesh *bm, BMLoop *l_sep);
BMVert *bmesh_kernel_unglue_region_make_vert_multi(BMesh *bm, BMLoop **larr, int larr_len);
BMVert *bmesh_kernel_unglue_region_make_vert_multi_isolated(BMesh *bm, BMLoop *l_sep);
