/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Duplicate geometry from one mesh from another.
 */

#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_math_vector.h"

#include "bmesh.h"
#include "intern/bmesh_private.h" /* for element checking */

static BMVert *bm_vert_copy(BMesh *bm_src, BMesh *bm_dst, BMVert *v_src)
{
  BMVert *v_dst = BM_vert_create(bm_dst, v_src->co, nullptr, BM_CREATE_SKIP_CD);
  BM_elem_attrs_copy(bm_src, bm_dst, v_src, v_dst);

  bm_elem_check_toolflags(bm_dst, (BMElem *)v_dst);

  return v_dst;
}

static BMEdge *bm_edge_copy_with_arrays(BMesh *bm_src,
                                        BMesh *bm_dst,
                                        BMEdge *e_src,
                                        BMVert **verts_dst)
{
  BMVert *e_dst_v1 = verts_dst[BM_elem_index_get(e_src->v1)];
  BMVert *e_dst_v2 = verts_dst[BM_elem_index_get(e_src->v2)];
  BMEdge *e_dst = BM_edge_create(bm_dst, e_dst_v1, e_dst_v2, nullptr, BM_CREATE_SKIP_CD);

  BM_elem_attrs_copy(bm_src, bm_dst, e_src, e_dst);

  bm_elem_check_toolflags(bm_dst, (BMElem *)e_dst);

  return e_dst;
}

static BMFace *bm_face_copy_with_arrays(
    BMesh *bm_src, BMesh *bm_dst, BMFace *f_src, BMVert **verts_dst, BMEdge **edges_dst)
{
  BMFace *f_dst;
  BMVert **vtar = BLI_array_alloca(vtar, f_src->len);
  BMEdge **edar = BLI_array_alloca(edar, f_src->len);
  BMLoop *l_iter_src, *l_iter_dst, *l_first_src;
  int i;

  l_first_src = BM_FACE_FIRST_LOOP(f_src);

  /* Lookup verts & edges. */
  l_iter_src = l_first_src;
  i = 0;
  do {
    vtar[i] = verts_dst[BM_elem_index_get(l_iter_src->v)];
    edar[i] = edges_dst[BM_elem_index_get(l_iter_src->e)];
    i++;
  } while ((l_iter_src = l_iter_src->next) != l_first_src);

  /* Create new face. */
  f_dst = BM_face_create(bm_dst, vtar, edar, f_src->len, nullptr, BM_CREATE_SKIP_CD);

  /* Copy attributes. */
  BM_elem_attrs_copy(bm_src, bm_dst, f_src, f_dst);

  bm_elem_check_toolflags(bm_dst, (BMElem *)f_dst);

  /* Copy per-loop custom data. */
  l_iter_src = l_first_src;
  l_iter_dst = BM_FACE_FIRST_LOOP(f_dst);
  do {
    BM_elem_attrs_copy(bm_src, bm_dst, l_iter_src, l_iter_dst);
  } while ((void)(l_iter_dst = l_iter_dst->next), (l_iter_src = l_iter_src->next) != l_first_src);

  return f_dst;
}

void BM_mesh_copy_arrays(BMesh *bm_src,
                         BMesh *bm_dst,
                         BMVert **verts_src,
                         uint verts_src_len,
                         BMEdge **edges_src,
                         uint edges_src_len,
                         BMFace **faces_src,
                         uint faces_src_len)
{
  /* Vertices. */
  BMVert **verts_dst = static_cast<BMVert **>(
      MEM_mallocN(sizeof(*verts_dst) * verts_src_len, __func__));
  for (uint i = 0; i < verts_src_len; i++) {
    BMVert *v_src = verts_src[i];
    BM_elem_index_set(v_src, i); /* set_dirty! */

    BMVert *v_dst = bm_vert_copy(bm_src, bm_dst, v_src);
    BM_elem_index_set(v_dst, i); /* set_ok */
    verts_dst[i] = v_dst;
  }
  bm_src->elem_index_dirty |= BM_VERT;
  bm_dst->elem_index_dirty &= ~BM_VERT;

  /* Edges. */
  BMEdge **edges_dst = static_cast<BMEdge **>(
      MEM_mallocN(sizeof(*edges_dst) * edges_src_len, __func__));
  for (uint i = 0; i < edges_src_len; i++) {
    BMEdge *e_src = edges_src[i];
    BM_elem_index_set(e_src, i); /* set_dirty! */

    BMEdge *e_dst = bm_edge_copy_with_arrays(bm_src, bm_dst, e_src, verts_dst);
    BM_elem_index_set(e_dst, i);
    edges_dst[i] = e_dst;
  }
  bm_src->elem_index_dirty |= BM_EDGE;
  bm_dst->elem_index_dirty &= ~BM_EDGE;

  /* Faces. */
  for (uint i = 0; i < faces_src_len; i++) {
    BMFace *f_src = faces_src[i];
    BMFace *f_dst = bm_face_copy_with_arrays(bm_src, bm_dst, f_src, verts_dst, edges_dst);
    BM_elem_index_set(f_dst, i);
  }
  bm_dst->elem_index_dirty &= ~BM_FACE;

  /* Cleanup. */
  MEM_freeN(verts_dst);
  MEM_freeN(edges_dst);
}
