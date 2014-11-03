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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/tools/bmesh_path.c
 *  \ingroup bmesh
 *
 * Find a path between 2 elements.
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"

#include "bmesh.h"
#include "bmesh_path.h"  /* own include */

/* -------------------------------------------------------------------- */
/* Generic Helpers */

static float step_cost_3_v3(const float v1[3], const float v2[3], const float v3[3])
{
	float cost, d1[3], d2[3];


	/* The cost is based on the simple sum of the length of the two edgees... */
	sub_v3_v3v3(d1, v2, v1);
	sub_v3_v3v3(d2, v3, v2);
	cost = normalize_v3(d1) + normalize_v3(d2);

	/* but is biased to give higher values to sharp turns, so that it will take
	 * paths with fewer "turns" when selecting between equal-weighted paths between
	 * the two edges */
	cost = cost * (1.0f + 0.5f * (2.0f - sqrtf(fabsf(dot_v3v3(d1, d2)))));

	return cost;
}



/* -------------------------------------------------------------------- */
/* BM_mesh_calc_path_vert */

static void verttag_add_adjacent(Heap *heap, BMVert *v_a, BMVert **verts_prev, float *cost, const bool use_length)
{
	BMIter eiter;
	BMEdge *e;
	BMVert *v_b;

	const int v_a_index = BM_elem_index_get(v_a);

	/* loop over faces of face, but do so by first looping over loops */
	BM_ITER_ELEM (e, &eiter, v_a, BM_EDGES_OF_VERT) {
		v_b = BM_edge_other_vert(e, v_a);
		if (!BM_elem_flag_test(v_b, BM_ELEM_TAG)) {
			/* we know 'f_b' is not visited, check it out! */
			const int v_b_index = BM_elem_index_get(v_b);
			const float cost_cut = use_length ? len_v3v3(v_a->co, v_b->co) : 1.0f;
			const float cost_new = cost[v_a_index] + cost_cut;

			if (cost[v_b_index] > cost_new) {
				cost[v_b_index] = cost_new;
				verts_prev[v_b_index] = v_a;
				BLI_heap_insert(heap, cost_new, v_b);
			}
		}
	}
}

LinkNode *BM_mesh_calc_path_vert(
        BMesh *bm, BMVert *v_src, BMVert *v_dst, const bool use_length,
        bool (*test_fn)(BMVert *, void *user_data), void *user_data)
{
	LinkNode *path = NULL;
	/* BM_ELEM_TAG flag is used to store visited edges */
	BMVert *v;
	BMIter viter;
	Heap *heap;
	float *cost;
	BMVert **verts_prev;
	int i, totvert;

	/* note, would pass BM_EDGE except we are looping over all faces anyway */
	// BM_mesh_elem_index_ensure(bm, BM_VERT /* | BM_EDGE */); // NOT NEEDED FOR FACETAG

	BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
		if (test_fn(v, user_data)) {
			BM_elem_flag_disable(v, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_enable(v, BM_ELEM_TAG);
		}

		BM_elem_index_set(v, i); /* set_inline */
	}
	bm->elem_index_dirty &= ~BM_VERT;

	/* alloc */
	totvert = bm->totvert;
	verts_prev = MEM_callocN(sizeof(*verts_prev) * totvert, __func__);
	cost = MEM_mallocN(sizeof(*cost) * totvert, __func__);

	fill_vn_fl(cost, totvert, 1e20f);

	/*
	 * Arrays are now filled as follows:
	 *
	 * As the search continues, verts_prev[n] will be the previous verts on the shortest
	 * path found so far to face n. BM_ELEM_TAG is used to tag elements we have visited,
	 * cost[n] will contain the length of the shortest
	 * path to face n found so far, Finally, heap is a priority heap which is built on the
	 * the same data as the cost array, but inverted: it is a worklist of faces prioritized
	 * by the shortest path found so far to the face.
	 */

	/* regular dijkstra shortest path, but over faces instead of vertices */
	heap = BLI_heap_new();
	BLI_heap_insert(heap, 0.0f, v_src);
	cost[BM_elem_index_get(v_src)] = 0.0f;

	while (!BLI_heap_is_empty(heap)) {
		v = BLI_heap_popmin(heap);

		if (v == v_dst)
			break;

		if (!BM_elem_flag_test(v, BM_ELEM_TAG)) {
			BM_elem_flag_enable(v, BM_ELEM_TAG);
			verttag_add_adjacent(heap, v, verts_prev, cost, use_length);
		}
	}

	if (v == v_dst) {
		do {
			BLI_linklist_prepend(&path, v);
		} while ((v = verts_prev[BM_elem_index_get(v)]));
	}

	MEM_freeN(verts_prev);
	MEM_freeN(cost);
	BLI_heap_free(heap, NULL);

	return path;
}



/* -------------------------------------------------------------------- */
/* BM_mesh_calc_path_edge */


static float edgetag_cut_cost(BMEdge *e1, BMEdge *e2, BMVert *v)
{
	BMVert *v1 = BM_edge_other_vert(e1, v);
	BMVert *v2 = BM_edge_other_vert(e2, v);
	return step_cost_3_v3(v1->co, v->co, v2->co);
}

static void edgetag_add_adjacent(Heap *heap, BMEdge *e1, BMEdge **edges_prev, float *cost, const bool use_length)
{
	BMIter viter;
	BMVert *v;

	BMIter eiter;
	BMEdge *e2;

	const int e1_index = BM_elem_index_get(e1);

	BM_ITER_ELEM (v, &viter, e1, BM_VERTS_OF_EDGE) {

		/* don't walk over previous vertex */
		if ((edges_prev[e1_index]) &&
		    (BM_vert_in_edge(edges_prev[e1_index], v)))
		{
			continue;
		}

		BM_ITER_ELEM (e2, &eiter, v, BM_EDGES_OF_VERT) {
			if (!BM_elem_flag_test(e2, BM_ELEM_TAG)) {
				/* we know 'e2' is not visited, check it out! */
				const int e2_index = BM_elem_index_get(e2);
				const float cost_cut = use_length ? edgetag_cut_cost(e1, e2, v) : 1.0f;
				const float cost_new = cost[e1_index] + cost_cut;

				if (cost[e2_index] > cost_new) {
					cost[e2_index] = cost_new;
					edges_prev[e2_index] = e1;
					BLI_heap_insert(heap, cost_new, e2);
				}
			}
		}
	}
}


LinkNode *BM_mesh_calc_path_edge(
        BMesh *bm, BMEdge *e_src, BMEdge *e_dst, const bool use_length,
        bool (*filter_fn)(BMEdge *, void *user_data), void *user_data)
{
	LinkNode *path = NULL;
	/* BM_ELEM_TAG flag is used to store visited edges */
	BMEdge *e;
	BMIter eiter;
	Heap *heap;
	float *cost;
	BMEdge **edges_prev;
	int i, totedge;

	/* note, would pass BM_EDGE except we are looping over all edges anyway */
	BM_mesh_elem_index_ensure(bm, BM_VERT /* | BM_EDGE */);

	BM_ITER_MESH_INDEX (e, &eiter, bm, BM_EDGES_OF_MESH, i) {
		if (filter_fn(e, user_data)) {
			BM_elem_flag_disable(e, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_enable(e, BM_ELEM_TAG);
		}

		BM_elem_index_set(e, i); /* set_inline */
	}
	bm->elem_index_dirty &= ~BM_EDGE;

	/* alloc */
	totedge = bm->totedge;
	edges_prev = MEM_callocN(sizeof(*edges_prev) * totedge, "SeamPathPrevious");
	cost = MEM_mallocN(sizeof(*cost) * totedge, "SeamPathCost");

	fill_vn_fl(cost, totedge, 1e20f);

	/*
	 * Arrays are now filled as follows:
	 *
	 * As the search continues, prevedge[n] will be the previous edge on the shortest
	 * path found so far to edge n. BM_ELEM_TAG is used to tag elements we have visited,
	 * cost[n] will contain the length of the shortest
	 * path to edge n found so far, Finally, heap is a priority heap which is built on the
	 * the same data as the cost array, but inverted: it is a worklist of edges prioritized
	 * by the shortest path found so far to the edge.
	 */

	/* regular dijkstra shortest path, but over edges instead of vertices */
	heap = BLI_heap_new();
	BLI_heap_insert(heap, 0.0f, e_src);
	cost[BM_elem_index_get(e_src)] = 0.0f;

	while (!BLI_heap_is_empty(heap)) {
		e = BLI_heap_popmin(heap);

		if (e == e_dst)
			break;

		if (!BM_elem_flag_test(e, BM_ELEM_TAG)) {
			BM_elem_flag_enable(e, BM_ELEM_TAG);
			edgetag_add_adjacent(heap, e, edges_prev, cost, use_length);
		}
	}

	if (e == e_dst) {
		do {
			BLI_linklist_prepend(&path, e);
		} while ((e = edges_prev[BM_elem_index_get(e)]));
	}

	MEM_freeN(edges_prev);
	MEM_freeN(cost);
	BLI_heap_free(heap, NULL);

	return path;
}



/* -------------------------------------------------------------------- */
/* BM_mesh_calc_path_face */

static float facetag_cut_cost(BMFace *f_a, BMFace *f_b, BMEdge *e)
{
	float f_a_cent[3];
	float f_b_cent[3];
	float e_cent[3];

	BM_face_calc_center_mean(f_a, f_a_cent);
	BM_face_calc_center_mean(f_b, f_b_cent);
	mid_v3_v3v3(e_cent, e->v1->co, e->v2->co);

	return step_cost_3_v3(f_a_cent, e_cent, f_b_cent);
}

static void facetag_add_adjacent(Heap *heap, BMFace *f_a, BMFace **faces_prev, float *cost, const bool use_length)
{
	BMIter liter;
	BMLoop *l_a;
	BMFace *f_b;

	const int f_a_index = BM_elem_index_get(f_a);

	/* loop over faces of face, but do so by first looping over loops */
	BM_ITER_ELEM (l_a, &liter, f_a, BM_LOOPS_OF_FACE) {
		BMLoop *l_first;
		BMLoop *l_iter;

		l_iter = l_first = l_a;
		do {
			f_b = l_iter->f;
			if (!BM_elem_flag_test(f_b, BM_ELEM_TAG)) {
				/* we know 'f_b' is not visited, check it out! */
				const int f_b_index = BM_elem_index_get(f_b);
				const float cost_cut = use_length ? facetag_cut_cost(f_a, f_b, l_iter->e) : 1.0f;
				const float cost_new = cost[f_a_index] + cost_cut;

				if (cost[f_b_index] > cost_new) {
					cost[f_b_index] = cost_new;
					faces_prev[f_b_index] = f_a;
					BLI_heap_insert(heap, cost_new, f_b);
				}
			}
		} while ((l_iter = l_iter->radial_next) != l_first);
	}
}

LinkNode *BM_mesh_calc_path_face(
        BMesh *bm, BMFace *f_src, BMFace *f_dst, const bool use_length,
        bool (*test_fn)(BMFace *, void *user_data), void *user_data)
{
	LinkNode *path = NULL;
	/* BM_ELEM_TAG flag is used to store visited edges */
	BMFace *f;
	BMIter fiter;
	Heap *heap;
	float *cost;
	BMFace **faces_prev;
	int i, totface;

	/* note, would pass BM_EDGE except we are looping over all faces anyway */
	// BM_mesh_elem_index_ensure(bm, BM_VERT /* | BM_EDGE */); // NOT NEEDED FOR FACETAG

	BM_ITER_MESH_INDEX (f, &fiter, bm, BM_FACES_OF_MESH, i) {
		if (test_fn(f, user_data)) {
			BM_elem_flag_disable(f, BM_ELEM_TAG);
		}
		else {
			BM_elem_flag_enable(f, BM_ELEM_TAG);
		}

		BM_elem_index_set(f, i); /* set_inline */
	}
	bm->elem_index_dirty &= ~BM_FACE;

	/* alloc */
	totface = bm->totface;
	faces_prev = MEM_callocN(sizeof(*faces_prev) * totface, __func__);
	cost = MEM_mallocN(sizeof(*cost) * totface, __func__);

	fill_vn_fl(cost, totface, 1e20f);

	/*
	 * Arrays are now filled as follows:
	 *
	 * As the search continues, faces_prev[n] will be the previous face on the shortest
	 * path found so far to face n. BM_ELEM_TAG is used to tag elements we have visited,
	 * cost[n] will contain the length of the shortest
	 * path to face n found so far, Finally, heap is a priority heap which is built on the
	 * the same data as the cost array, but inverted: it is a worklist of faces prioritized
	 * by the shortest path found so far to the face.
	 */

	/* regular dijkstra shortest path, but over faces instead of vertices */
	heap = BLI_heap_new();
	BLI_heap_insert(heap, 0.0f, f_src);
	cost[BM_elem_index_get(f_src)] = 0.0f;

	while (!BLI_heap_is_empty(heap)) {
		f = BLI_heap_popmin(heap);

		if (f == f_dst)
			break;

		if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
			BM_elem_flag_enable(f, BM_ELEM_TAG);
			facetag_add_adjacent(heap, f, faces_prev, cost, use_length);
		}
	}

	if (f == f_dst) {
		do {
			BLI_linklist_prepend(&path, f);
		} while ((f = faces_prev[BM_elem_index_get(f)]));
	}

	MEM_freeN(faces_prev);
	MEM_freeN(cost);
	BLI_heap_free(heap, NULL);

	return path;
}
