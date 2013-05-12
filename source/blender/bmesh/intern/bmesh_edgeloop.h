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
 * The Original Code is Copyright (C) 2013 by Campbell Barton.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_EDGELOOP_H__
#define __BMESH_EDGELOOP_H__

/** \file blender/bmesh/intern/bmesh_edgeloop.h
 *  \ingroup bmesh
 */

struct ListBase;
struct BMEdgeLoopStore;

/* multiple edgeloops (ListBase) */
int                 BM_mesh_edgeloops_find(BMesh *bm, struct ListBase *r_lb,
                                           bool (*test_fn)(BMEdge *, void *user_data), void *user_data);

void                BM_mesh_edgeloops_free(struct ListBase *eloops);
void                BM_mesh_edgeloops_calc_center(BMesh *bm, struct ListBase *eloops);
void                BM_mesh_edgeloops_calc_normal(BMesh *bm, struct ListBase *eloops);
void                BM_mesh_edgeloops_calc_order(BMesh *UNUSED(bm), ListBase *eloops, const bool use_normals);


/* single edgeloop */
struct BMEdgeLoopStore *BM_edgeloop_copy(struct BMEdgeLoopStore *el_store);
void                BM_edgeloop_free(struct BMEdgeLoopStore *el_store);
bool                BM_edgeloop_is_closed(struct BMEdgeLoopStore *el_store);
int                 BM_edgeloop_length_get(struct BMEdgeLoopStore *el_store);
struct ListBase    *BM_edgeloop_verts_get(struct BMEdgeLoopStore *el_store);
const float        *BM_edgeloop_normal_get(struct BMEdgeLoopStore *el_store);
const float        *BM_edgeloop_center_get(struct BMEdgeLoopStore *el_store);
void                BM_edgeloop_calc_center(BMesh *bm, struct BMEdgeLoopStore *el_store);
void                BM_edgeloop_calc_normal(BMesh *bm, struct BMEdgeLoopStore *el_store);
void                BM_edgeloop_flip(BMesh *bm, struct BMEdgeLoopStore *el_store);
void                BM_edgeloop_expand(BMesh *bm, struct BMEdgeLoopStore *el_store, int el_store_len);

#define BM_EDGELOOP_NEXT(el_store, elink) \
	(elink)->next ? elink->next : (BM_edgeloop_is_closed(el_store) ? BM_edgeloop_verts_get(el_store)->first : NULL)

#endif  /* __BMESH_EDGELOOP_H__ */
