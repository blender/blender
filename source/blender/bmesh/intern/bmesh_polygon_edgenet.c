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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_polygon_edgenet.c
 *  \ingroup bmesh
 *
 * This file contains functions for splitting faces into isolated regions,
 * defined by connected edges.
 */
// #define DEBUG_PRINT

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_alloca.h"
#include "BLI_stackdefines.h"
#include "BLI_linklist_stack.h"
#include "BLI_sort_utils.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* -------------------------------------------------------------------- */
/* Face Split Edge-Net */

/** \name BM_face_split_edgenet and helper functions.
 *
 * \note Don't use #BM_edge_is_wire or #BM_edge_is_boundary
 * since we need to take flagged faces into account.
 * Also take care accessing e->l directly.
 *
 * \{ */

/* Note: All these flags _must_ be cleared on exit */

/* face is apart of the edge-net (including the original face we're splitting) */
#define FACE_NET  _FLAG_WALK
/* edge is apart of the edge-net we're filling */
#define EDGE_NET   _FLAG_WALK
/* tag verts we've visit */
#define VERT_VISIT _FLAG_WALK

struct VertOrder {
	float   angle;
	BMVert *v;
};

static unsigned int bm_edge_flagged_radial_count(BMEdge *e)
{
	unsigned int count = 0;
	BMLoop *l;

	if ((l = e->l)) {
		do {
			if (BM_ELEM_API_FLAG_TEST(l->f, FACE_NET)) {
				count++;
			}
		} while ((l = l->radial_next) != e->l);
	}
	return count;
}

static BMLoop *bm_edge_flagged_radial_first(BMEdge *e)
{
	BMLoop *l;

	if ((l = e->l)) {
		do {
			if (BM_ELEM_API_FLAG_TEST(l->f, FACE_NET)) {
				return l;
			}
		} while ((l = l->radial_next) != e->l);
	}
	return NULL;
}

static bool bm_face_split_edgenet_find_loop_pair(
        BMVert *v_init, const float face_normal[3],
        BMEdge *e_pair[2])
{
	/* Always find one boundary edge (to determine winding)
	 * and one wire (if available), otherwise another boundary.
	 */
	BMIter iter;
	BMEdge *e;

	/* detect winding */
	BMLoop *l_walk;
	bool swap;

	BLI_SMALLSTACK_DECLARE(edges_boundary, BMEdge *);
	BLI_SMALLSTACK_DECLARE(edges_wire,     BMEdge *);
	int edges_boundary_len = 0;
	int edges_wire_len = 0;

	BM_ITER_ELEM (e, &iter, v_init, BM_EDGES_OF_VERT) {
		if (BM_ELEM_API_FLAG_TEST(e, EDGE_NET)) {
			const unsigned int count = bm_edge_flagged_radial_count(e);
			if (count == 1) {
				BLI_SMALLSTACK_PUSH(edges_boundary, e);
				edges_boundary_len++;
			}
			else if (count == 0) {
				BLI_SMALLSTACK_PUSH(edges_wire, e);
				edges_wire_len++;
			}
		}
	}

	/* first edge should always be boundary */
	if (edges_boundary_len == 0) {
		return false;
	}
	e_pair[0] = BLI_SMALLSTACK_POP(edges_boundary);

	/* attempt one boundary and one wire, or 2 boundary */
	if (edges_wire_len == 0) {
		if (edges_boundary_len >= 2) {
			e_pair[1] = BLI_SMALLSTACK_POP(edges_boundary);
		}
		else {
			/* one boundary and no wire */
			return false;
		}
	}
	else {
		e_pair[1] = BLI_SMALLSTACK_POP(edges_wire);

		if (edges_wire_len > 1) {
			BMVert *v_prev = BM_edge_other_vert(e_pair[0], v_init);
			BMVert *v_next;
			float angle_best;

			v_next = BM_edge_other_vert(e_pair[1], v_init);
			angle_best = angle_on_axis_v3v3v3_v3(v_prev->co, v_init->co, v_next->co, face_normal);

			while ((e = BLI_SMALLSTACK_POP(edges_wire))) {
				float angle_test;
				v_next = BM_edge_other_vert(e, v_init);
				angle_test = angle_on_axis_v3v3v3_v3(v_prev->co, v_init->co, v_next->co, face_normal);
				if (angle_test < angle_best) {
					angle_best = angle_test;
					e_pair[1] = e;
				}
			}
		}
	}


	/* flip based on winding */
	l_walk = bm_edge_flagged_radial_first(e_pair[0]);
	swap = false;
	if (face_normal == l_walk->f->no) {
		swap = !swap;
	}
	if (l_walk->v != v_init) {
		swap = !swap;
	}
	if (swap) {
		SWAP(BMEdge *, e_pair[0], e_pair[1]);
	}

	return true;
}

static bool bm_face_split_edgenet_find_loop_walk(
        BMVert *v_init, const float face_normal[3],
        /* cache to avoid realloc every time */
        struct VertOrder *edge_order, const unsigned int edge_order_len,
        BMEdge *e_pair[2])
{
	/* fast-path for the common case (avoid push-pop).
	 * Also avoids tagging as visited since we know we
	 * can't reach these verts some other way */
#define USE_FASTPATH_NOFORK

	BMVert *v;
	BMVert *v_dst;
	bool found = false;

	struct VertOrder *eo;
	STACK_DECLARE(edge_order);

	/* store visited verts so we can clear the visit flag after execution */
	BLI_SMALLSTACK_DECLARE(vert_visit, BMVert *);

	/* likely this will stay very small
	 * all verts pushed into this stack _must_ have their previous edges set! */
	BLI_SMALLSTACK_DECLARE(vert_stack, BMVert *);
	BLI_SMALLSTACK_DECLARE(vert_stack_next, BMVert *);

	STACK_INIT(edge_order, edge_order_len);

	/* start stepping */
	v = BM_edge_other_vert(e_pair[0], v_init);
	v->e = e_pair[0];
	BLI_SMALLSTACK_PUSH(vert_stack, v);

	v_dst = BM_edge_other_vert(e_pair[1], v_init);

#ifdef DEBUG_PRINT
	printf("%s: vert (search) %d\n", __func__, BM_elem_index_get(v_init));
#endif

	/* This loop will keep stepping over the best possible edge,
	 * in most cases it finds the direct route to close the face.
	 *
	 * In cases where paths can't be closed,
	 * alternatives are stored in the 'vert_stack'.
	 */
	while ((v = BLI_SMALLSTACK_POP_EX(vert_stack, vert_stack_next))) {
		BMIter eiter;
		BMEdge *e_next;

#ifdef USE_FASTPATH_NOFORK
walk_nofork:
#else
		BLI_SMALLSTACK_PUSH(vert_visit, v);
		BM_ELEM_API_FLAG_ENABLE(v, VERT_VISIT);
#endif

		BLI_assert(STACK_SIZE(edge_order) == 0);

		/* check if we're done! */
		if (v == v_dst) {
			found = true;
			goto finally;
		}

		BM_ITER_ELEM (e_next, &eiter, v, BM_EDGES_OF_VERT) {
			if ((v->e != e_next) &&
			    (BM_ELEM_API_FLAG_TEST(e_next, EDGE_NET)) &&
			    (bm_edge_flagged_radial_count(e_next) < 2))
			{
				BMVert *v_next;

				v_next = BM_edge_other_vert(e_next, v);

#ifdef DEBUG_PRINT
				/* indent and print */
				{
					BMVert *_v = v;
					do {
						printf("  ");
					} while ((_v = BM_edge_other_vert(_v->e, _v)) != v_init);
					printf("vert %d -> %d (add=%d)\n",
					       BM_elem_index_get(v), BM_elem_index_get(v_next),
					       BM_ELEM_API_FLAG_TEST(v_next, VERT_VISIT) == 0);
				}
#endif

				if (!BM_ELEM_API_FLAG_TEST(v_next, VERT_VISIT)) {
					eo = STACK_PUSH_RET_PTR(edge_order);
					eo->v = v_next;

					v_next->e = e_next;
				}
			}
		}

#ifdef USE_FASTPATH_NOFORK
		if (STACK_SIZE(edge_order) == 1) {
			eo = STACK_POP_PTR(edge_order);
			v = eo->v;

			goto walk_nofork;
		}
#endif

		/* sort by angle if needed */
		if (STACK_SIZE(edge_order) > 1) {
			unsigned int j;
			BMVert *v_prev = BM_edge_other_vert(v->e, v);

			for (j = 0; j < STACK_SIZE(edge_order); j++) {
				edge_order[j].angle = angle_signed_on_axis_v3v3v3_v3(v_prev->co, v->co, edge_order[j].v->co, face_normal);
			}
			qsort(edge_order, STACK_SIZE(edge_order), sizeof(struct VertOrder), BLI_sortutil_cmp_float_reverse);

#ifdef USE_FASTPATH_NOFORK
			/* only tag forks */
			BLI_SMALLSTACK_PUSH(vert_visit, v);
			BM_ELEM_API_FLAG_ENABLE(v, VERT_VISIT);
#endif
		}

		while ((eo = STACK_POP_PTR(edge_order))) {
			BLI_SMALLSTACK_PUSH(vert_stack_next, eo->v);
		}

		if (!BLI_SMALLSTACK_IS_EMPTY(vert_stack_next)) {
			BLI_SMALLSTACK_SWAP(vert_stack, vert_stack_next);
		}
	}


finally:
	/* clear flag for next execution */
	while ((v = BLI_SMALLSTACK_POP(vert_visit))) {
		BM_ELEM_API_FLAG_DISABLE(v, VERT_VISIT);
	}

	return found;

#undef USE_FASTPATH_NOFORK
}

static bool bm_face_split_edgenet_find_loop(
        BMVert *v_init, const float face_normal[3],
        /* cache to avoid realloc every time */
        struct VertOrder *edge_order, const unsigned int edge_order_len,
        BMVert **r_face_verts, int *r_face_verts_len)
{
	BMEdge *e_pair[2];
	BMVert *v;

	if (!bm_face_split_edgenet_find_loop_pair(v_init, face_normal, e_pair)) {
		return false;
	}

	BLI_assert((bm_edge_flagged_radial_count(e_pair[0]) == 1) ||
	           (bm_edge_flagged_radial_count(e_pair[1]) == 1));

	if (bm_face_split_edgenet_find_loop_walk(v_init, face_normal, edge_order, edge_order_len, e_pair)) {
		unsigned int i = 0;

		r_face_verts[i++] = v_init;
		v = BM_edge_other_vert(e_pair[1], v_init);
		do {
			r_face_verts[i++] = v;
		} while ((v = BM_edge_other_vert(v->e, v)) != v_init);
		*r_face_verts_len = i;
		return (i > 2) ? true : false;
	}
	else {
		return false;
	}
}

/**
 * Splits a face into many smaller faces defined by an edge-net.
 * handle customdata and degenerate cases.
 *
 * - isolated holes or unsupported face configurations, will be ignored.
 * - customdata calculations aren't efficient
 *   (need to calculate weights for each vert).
 */
bool BM_face_split_edgenet(
        BMesh *bm,
        BMFace *f, BMEdge **edge_net, const int edge_net_len,
        BMFace ***r_face_arr, int *r_face_arr_len)
{
	/* re-use for new face verts */
	BMVert **face_verts;
	int      face_verts_len;

	BMFace **face_arr = NULL;
	BLI_array_declare(face_arr);

	BMVert **vert_queue;
	STACK_DECLARE(vert_queue);
	int i;

	struct VertOrder *edge_order;
	const unsigned int edge_order_len = edge_net_len + 2;

	BMVert *v;

	BMLoop *l_iter, *l_first;


	if (!edge_net_len) {
		if (r_face_arr) {
			*r_face_arr = NULL;
			*r_face_arr_len = 0;
		}
		return false;
	}

	/* over-alloc (probably 2-4 is only used in most cases), for the biggest-fan */
	edge_order = BLI_array_alloca(edge_order, edge_order_len);

	/* use later */
	face_verts = BLI_array_alloca(face_verts, edge_net_len + f->len);

	vert_queue = BLI_array_alloca(vert_queue, edge_net_len + f->len);
	STACK_INIT(vert_queue, f->len + edge_net_len);

	BLI_assert(BM_ELEM_API_FLAG_TEST(f, FACE_NET) == 0);
	BM_ELEM_API_FLAG_ENABLE(f, FACE_NET);

#ifdef DEBUG
	for (i = 0; i < edge_net_len; i++) {
		BLI_assert(BM_ELEM_API_FLAG_TEST(edge_net[i], EDGE_NET) == 0);
		BLI_assert(BM_edge_in_face(edge_net[i], f) == false);
	}
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BLI_assert(BM_ELEM_API_FLAG_TEST(l_iter->e, EDGE_NET) == 0);
	} while ((l_iter = l_iter->next) != l_first);
#endif


	for (i = 0; i < edge_net_len; i++) {
		BM_ELEM_API_FLAG_ENABLE(edge_net[i], EDGE_NET);
	}
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BM_ELEM_API_FLAG_ENABLE(l_iter->e, EDGE_NET);
	} while ((l_iter = l_iter->next) != l_first);


	/* any vert can be used to begin with */
	STACK_PUSH(vert_queue, l_first->v);

	while ((v = STACK_POP(vert_queue))) {
		if (bm_face_split_edgenet_find_loop(v, f->no, edge_order, edge_order_len, face_verts, &face_verts_len)) {
			BMFace *f_new;

			f_new = BM_face_create_verts(bm, face_verts, face_verts_len, f, BM_CREATE_NOP, false);

			for (i = 0; i < edge_net_len; i++) {
				BLI_assert(BM_ELEM_API_FLAG_TEST(edge_net[i], EDGE_NET));
			}

			if (f_new) {
				bool l_prev_is_boundary;
				BLI_array_append(face_arr, f_new);
				copy_v3_v3(f_new->no, f->no);

				BM_ELEM_API_FLAG_ENABLE(f_new, FACE_NET);

				/* add new verts to keep finding loops for
				 * (verts between boundary and manifold edges) */
				l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
				l_prev_is_boundary = (bm_edge_flagged_radial_count(l_iter->prev->e) == 1);
				do {
					bool l_iter_is_boundary = (bm_edge_flagged_radial_count(l_iter->e) == 1);
					if (l_prev_is_boundary != l_iter_is_boundary) {
						STACK_PUSH(vert_queue, l_iter->v);
					}
					l_prev_is_boundary = l_iter_is_boundary;
				} while ((l_iter = l_iter->next) != l_first);
			}
		}
	}


	if (CustomData_has_math(&bm->ldata)) {
		/* reuse VERT_VISIT here to tag vert's already interpolated */
		BMIter iter;
		BMLoop *l_other;

		/* see: #BM_loop_interp_from_face for similar logic  */
		void **blocks   = BLI_array_alloca(blocks, f->len);
		float (*cos_2d)[2] = BLI_array_alloca(cos_2d, f->len);
		float *w        = BLI_array_alloca(w, f->len);
		float axis_mat[3][3];
		float co[2];

		/* interior loops */
		axis_dominant_v3_to_m3(axis_mat, f->no);


		/* first simply copy from existing face */
		i = 0;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BM_ITER_ELEM (l_other, &iter, l_iter->v, BM_LOOPS_OF_VERT) {
				if ((l_other->f != f) && BM_ELEM_API_FLAG_TEST(l_other->f, FACE_NET)) {
					CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata,
					                           l_iter->head.data, &l_other->head.data);
				}
			}
			/* tag not to interpolate */
			BM_ELEM_API_FLAG_ENABLE(l_iter->v, VERT_VISIT);


			mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
			blocks[i] = l_iter->head.data;

		} while (i++, (l_iter = l_iter->next) != l_first);


		for (i = 0; i < edge_net_len; i++) {
			BM_ITER_ELEM (v, &iter, edge_net[i], BM_VERTS_OF_EDGE) {
				if (!BM_ELEM_API_FLAG_TEST(v, VERT_VISIT)) {
					BMIter liter;

					BM_ELEM_API_FLAG_ENABLE(v, VERT_VISIT);

					/* interpolate this loop, then copy to the rest */
					l_first = NULL;

					BM_ITER_ELEM (l_iter, &liter, v, BM_LOOPS_OF_VERT) {
						if (BM_ELEM_API_FLAG_TEST(l_iter->f, FACE_NET)) {
							if (l_first == NULL) {
								mul_v2_m3v3(co, axis_mat, v->co);
								interp_weights_poly_v2(w, cos_2d, f->len, co);
								CustomData_bmesh_interp(
								        &bm->ldata, (const void **)blocks,
								        w, NULL, f->len, l_iter->head.data);
								l_first = l_iter;
							}
							else {
								CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata,
								                           l_first->head.data, &l_iter->head.data);
							}
						}
					}
				}
			}
		}
	}



	/* cleanup */
	for (i = 0; i < edge_net_len; i++) {
		BM_ELEM_API_FLAG_DISABLE(edge_net[i], EDGE_NET);
		/* from interp only */
		BM_ELEM_API_FLAG_DISABLE(edge_net[i]->v1, VERT_VISIT);
		BM_ELEM_API_FLAG_DISABLE(edge_net[i]->v2, VERT_VISIT);
	}
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BM_ELEM_API_FLAG_DISABLE(l_iter->e, EDGE_NET);
		/* from interp only */
		BM_ELEM_API_FLAG_DISABLE(l_iter->v, VERT_VISIT);
	} while ((l_iter = l_iter->next) != l_first);

	if (BLI_array_count(face_arr)) {
		bmesh_face_swap_data(f, face_arr[0]);
		BM_face_kill(bm, face_arr[0]);
		face_arr[0] = f;
	}
	else {
		BM_ELEM_API_FLAG_DISABLE(f, FACE_NET);
	}

	for (i = 0; i < BLI_array_count(face_arr); i++) {
		BM_ELEM_API_FLAG_DISABLE(face_arr[i], FACE_NET);
	}

	if (r_face_arr) {
		*r_face_arr = face_arr;
		*r_face_arr_len = BLI_array_count(face_arr);
	}
	else {
		if (face_arr) {
			MEM_freeN(face_arr);
		}
	}

	return true;
}

#undef FACE_NET
#undef VERT_VISIT
#undef EDGE_NET

/** \} */
