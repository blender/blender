/* SPDX-FileCopyrightText: 2013 by Campbell Barton. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "bmesh_class.hh"

struct BMEdgeLoopStore;
struct GSet;
struct ListBase;

/* multiple edgeloops (ListBase) */
/**
 * \return listbase of listbases, each linking to a vertex.
 */
int BM_mesh_edgeloops_find(BMesh *bm,
                           ListBase *r_eloops,
                           bool (*test_fn)(BMEdge *, void *user_data),
                           void *user_data);
bool BM_mesh_edgeloops_find_path(BMesh *bm,
                                 ListBase *r_eloops,
                                 bool (*test_fn)(BMEdge *, void *user_data),
                                 void *user_data,
                                 BMVert *v_src,
                                 BMVert *v_dst);

void BM_mesh_edgeloops_free(ListBase *eloops);
void BM_mesh_edgeloops_calc_center(BMesh *bm, ListBase *eloops);
void BM_mesh_edgeloops_calc_normal(BMesh *bm, ListBase *eloops);
void BM_mesh_edgeloops_calc_normal_aligned(BMesh *bm, ListBase *eloops, const float no_align[3]);
void BM_mesh_edgeloops_calc_order(BMesh *bm, ListBase *eloops, bool use_normals);

/**
 * Copy a single edge-loop.
 * \return new edge-loops.
 */
BMEdgeLoopStore *BM_edgeloop_copy(BMEdgeLoopStore *el_store);
BMEdgeLoopStore *BM_edgeloop_from_verts(BMVert **v_arr, int v_arr_tot, bool is_closed);

void BM_edgeloop_free(BMEdgeLoopStore *el_store);
bool BM_edgeloop_is_closed(BMEdgeLoopStore *el_store);
int BM_edgeloop_length_get(BMEdgeLoopStore *el_store);
ListBase *BM_edgeloop_verts_get(BMEdgeLoopStore *el_store);
const float *BM_edgeloop_normal_get(BMEdgeLoopStore *el_store);
const float *BM_edgeloop_center_get(BMEdgeLoopStore *el_store);
/**
 * Edges are assigned to one vert -> the next.
 */
void BM_edgeloop_edges_get(BMEdgeLoopStore *el_store, BMEdge **e_arr);
void BM_edgeloop_calc_center(BMesh *bm, BMEdgeLoopStore *el_store);
bool BM_edgeloop_calc_normal(BMesh *bm, BMEdgeLoopStore *el_store);
/**
 * For open loops that are straight lines,
 * calculating the normal as if it were a polygon is meaningless.
 *
 * Instead use an alignment vector and calculate the normal based on that.
 */
bool BM_edgeloop_calc_normal_aligned(BMesh *bm,
                                     BMEdgeLoopStore *el_store,
                                     const float no_align[3]);
void BM_edgeloop_flip(BMesh *bm, BMEdgeLoopStore *el_store);
void BM_edgeloop_expand(
    BMesh *bm, BMEdgeLoopStore *el_store, int el_store_len, bool split, GSet *split_edges);

bool BM_edgeloop_overlap_check(BMEdgeLoopStore *el_store_a, BMEdgeLoopStore *el_store_b);

#define BM_EDGELINK_NEXT(el_store, elink) \
  (elink)->next ? \
      (elink)->next : \
      (BM_edgeloop_is_closed(el_store) ? (LinkData *)BM_edgeloop_verts_get(el_store)->first : \
                                         NULL)

#define BM_EDGELOOP_NEXT(el_store) \
  (CHECK_TYPE_INLINE(el_store, BMEdgeLoopStore *), (BMEdgeLoopStore *)((LinkData *)el_store)->next)
