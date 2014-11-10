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
 * Contributor(s): Campbell Barton.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_connect_pair.c
 *  \ingroup bmesh
 *
 * Connect vertex pair across multiple faces (splits faces).
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#include "BLI_mempool.h"
#include "BLI_listbase.h"

/**
 * Method for connecting across many faces.
 *
 * - use the line between both verts and their normal average to construct a matrix.
 * - using the matrix, we can find all intersecting verts/edges and build connection data.
 * - then walk the connected data and find the shortest path (as we do with other shortest-path functions).
 * - if the connection can't be found - fail.
 * - with the connection found, split all edges tagging verts (or tag verts that sit on the intersection).
 * - run the standard connect operator.
 */

#define CONNECT_EPS 0.0001f
#define VERT_OUT 1
#define VERT_EXCLUDE 2

/* typically hidden faces */
#define FACE_EXCLUDE 2

#define FACE_WALK_TEST(f)  (CHECK_TYPE_INLINE(f, BMFace *), \
	BMO_elem_flag_test(pc->bm_bmoflag, f, FACE_EXCLUDE) == 0)
#define VERT_WALK_TEST(v)  (CHECK_TYPE_INLINE(v, BMVert *), \
	BMO_elem_flag_test(pc->bm_bmoflag, v, VERT_EXCLUDE) == 0)

// #define DEBUG_PRINT

typedef struct PathContext {
	ListBase state_lb;
	float matrix[3][3];
	float axis_sep;

	/* only to access BMO flags */
	BMesh *bm_bmoflag;

	BMVert *v_a, *v_b;

	BLI_mempool *link_pool;
} PathContext;

/**
 * Single linked list where each item contains state and points to previous path item.
 */
typedef struct PathLink {
	struct PathLink *next;
	BMElem *ele;       /* edge or vert */
	BMElem *ele_from;  /* edge or face we came from (not 'next->ele') */
} PathLink;

typedef struct PathLinkState {
	struct PathLinkState *next, *prev;

	/* chain of links */
	struct PathLink *link_last;

	/* length along links */
	float dist;
	float co_prev[3];
} PathLinkState;

static int state_isect_co_pair(const PathContext *pc,
                               const float co_a[3], const float co_b[3])
{
	const float diff_a = dot_m3_v3_row_x((float (*)[3])pc->matrix, co_a) - pc->axis_sep;
	const float diff_b = dot_m3_v3_row_x((float (*)[3])pc->matrix, co_b) - pc->axis_sep;

	const int test_a = (fabsf(diff_a) < CONNECT_EPS) ? 0 : (diff_a < 0.0f) ? -1 : 1;
	const int test_b = (fabsf(diff_b) < CONNECT_EPS) ? 0 : (diff_b < 0.0f) ? -1 : 1;

	if ((test_a && test_b) && (test_a != test_b)) {
		return 1;  /* on either side */
	}
	else {
		return 0;
	}
}

static int state_isect_co_exact(const PathContext *pc,
                                const float co[3])
{
	const float diff = dot_m3_v3_row_x((float (*)[3])pc->matrix, co) - pc->axis_sep;
	return (fabsf(diff) <= CONNECT_EPS);
}

static float state_calc_co_pair_fac(const PathContext *pc,
                                    const float co_a[3], const float co_b[3])
{
	float diff_a, diff_b, diff_tot;

	diff_a = fabsf(dot_m3_v3_row_x((float (*)[3])pc->matrix, co_a) - pc->axis_sep);
	diff_b = fabsf(dot_m3_v3_row_x((float (*)[3])pc->matrix, co_b) - pc->axis_sep);
	diff_tot = (diff_a + diff_b);
	return (diff_tot > FLT_EPSILON) ? (diff_a / diff_tot) : 0.5f;
}

static void state_calc_co_pair(const PathContext *pc,
                               const float co_a[3], const float co_b[3],
                               float r_co[3])
{
	const float fac = state_calc_co_pair_fac(pc, co_a, co_b);
	interp_v3_v3v3(r_co, co_a, co_b, fac);
}

/**
 * Ideally we wouldn't need this and for most cases we don't.
 * But when a face has vertices that are on the boundary more then once this becomes tricky.
 */
static bool state_link_find(PathLinkState *state, BMElem *ele)
{
	PathLink *link = state->link_last;
	BLI_assert(ELEM(ele->head.htype, BM_VERT, BM_EDGE, BM_FACE));
	if (link) {
		do {
			if (link->ele == ele) {
				return true;
			}
		} while ((link = link->next));
	}
	return false;
}

static void state_link_add(PathContext *pc, PathLinkState *state,
                           BMElem *ele, BMElem *ele_from)
{
	PathLink *step_new = BLI_mempool_alloc(pc->link_pool);
	BLI_assert(ele != ele_from);
	BLI_assert(state_link_find(state, ele) == false);

#ifdef DEBUG_PRINT
	printf("%s: adding to state %p:%d, %.4f - ", __func__, state, BLI_findindex(&pc->state_lb, state), state->dist);
	if (ele->head.htype == BM_VERT) {
		printf("vert %d, ", BM_elem_index_get(ele));
	}
	else if (ele->head.htype == BM_EDGE) {
		printf("edge %d, ", BM_elem_index_get(ele));
	}
	else {
		BLI_assert(0);
	}

	if (ele_from == NULL) {
		printf("from NULL\n");
	}
	else if (ele_from->head.htype == BM_EDGE) {
		printf("from edge %d\n", BM_elem_index_get(ele_from));
	}
	else if (ele_from->head.htype == BM_FACE) {
		printf("from face %d\n", BM_elem_index_get(ele_from));
	}
	else {
		BLI_assert(0);
	}
#endif

	/* track distance */
	{
		float co[3];
		if (ele->head.htype == BM_VERT) {
			copy_v3_v3(co, ((BMVert *)ele)->co);
		}
		else if (ele->head.htype == BM_EDGE) {
			state_calc_co_pair(pc, ((BMEdge *)ele)->v1->co, ((BMEdge *)ele)->v2->co, co);
		}
		else {
			BLI_assert(0);
		}

		/* tally distance */
		if (ele_from) {
			state->dist += len_v3v3(state->co_prev, co);
		}
		copy_v3_v3(state->co_prev, co);
	}

	step_new->ele = ele;
	step_new->ele_from = ele_from;
	step_new->next = state->link_last;
	state->link_last = step_new;
}

static PathLinkState *state_dupe_add(
        PathContext *pc,
        PathLinkState *state, const PathLinkState *state_orig)
{
	state = MEM_mallocN(sizeof(*state), __func__);
	*state = *state_orig;
	BLI_addhead(&pc->state_lb, state);
	return state;
}

/* walk around the face edges */
static PathLinkState *state_step__face_edges(
        PathContext *pc,
        PathLinkState *state, const PathLinkState *state_orig,
        BMLoop *l_iter, BMLoop *l_last,
        float *r_dist_best)
{
	BMLoop *l_iter_best = NULL;
	float dist_best = *r_dist_best;

	do {
		if (state_isect_co_pair(pc, l_iter->v->co, l_iter->next->v->co)) {
			float dist_test;
			float co_isect[3];

			state_calc_co_pair(pc, l_iter->v->co, l_iter->next->v->co, co_isect);
			dist_test = len_squared_v3v3(state->co_prev, co_isect);
			if (dist_test < dist_best) {
				BMElem *ele_next      = (BMElem *)l_iter->e;
				BMElem *ele_next_from = (BMElem *)l_iter->f;

				if (FACE_WALK_TEST((BMFace *)ele_next_from) &&
				    (state_link_find(state, ele_next) == false))
				{
					dist_best = dist_test;
					l_iter_best = l_iter;
				}
			}
		}
	} while ((l_iter = l_iter->next) != l_last);

	if ((l_iter = l_iter_best)) {
		BMElem *ele_next      = (BMElem *)l_iter->e;
		BMElem *ele_next_from = (BMElem *)l_iter->f;

		if (state_orig->link_last != state->link_last) {
			state = state_dupe_add(pc, state, state_orig);
		}
		state_link_add(pc, state, ele_next, ele_next_from);
	}

	*r_dist_best = dist_best;

	return state;
}

/* walk around the face verts */
static PathLinkState *state_step__face_verts(
        PathContext *pc,
        PathLinkState *state, const PathLinkState *state_orig,
        BMLoop *l_iter, BMLoop *l_last, float *r_dist_best)
{
	BMLoop *l_iter_best = NULL;
	float dist_best = *r_dist_best;

	do {
		if (state_isect_co_exact(pc, l_iter->v->co)) {
			const float dist_test = len_squared_v3v3(state->co_prev, l_iter->v->co);
			if (dist_test < dist_best) {
				BMElem *ele_next      = (BMElem *)l_iter->v;
				BMElem *ele_next_from = (BMElem *)l_iter->f;

				if (FACE_WALK_TEST((BMFace *)ele_next_from) &&
				    state_link_find(state, ele_next) == false)
				{
					dist_best = dist_test;
					l_iter_best = l_iter;
				}
			}
		}
	} while ((l_iter = l_iter->next) != l_last);

	if ((l_iter = l_iter_best)) {
		BMElem *ele_next      = (BMElem *)l_iter->v;
		BMElem *ele_next_from = (BMElem *)l_iter->f;

		if (state_orig->link_last != state->link_last) {
			state = state_dupe_add(pc, state, state_orig);
		}
		state_link_add(pc, state, ele_next, ele_next_from);
	}

	*r_dist_best = dist_best;

	return state;
}

static bool state_step(PathContext *pc, PathLinkState *state)
{
	PathLinkState state_orig = *state;
	BMElem *ele = state->link_last->ele;
	const void *ele_from = state->link_last->ele_from;

	if (ele->head.htype == BM_EDGE) {
		BMEdge *e = (BMEdge *)ele;

		BMIter liter;
		BMLoop *l_start;

		BM_ITER_ELEM (l_start, &liter, e, BM_LOOPS_OF_EDGE) {
			if ((l_start->f != ele_from) &&
			    FACE_WALK_TEST(l_start->f))
			{
				float dist_best = FLT_MAX;
				/* very similar to block below */
				state = state_step__face_edges(pc, state, &state_orig,
				                               l_start->next, l_start, &dist_best);
				state = state_step__face_verts(pc, state, &state_orig,
				                               l_start->next->next, l_start, &dist_best);
			}
		}
	}
	else if (ele->head.htype == BM_VERT) {
		BMVert *v = (BMVert *)ele;

		/* vert loops */
		{
			BMIter liter;
			BMLoop *l_start;

			BM_ITER_ELEM (l_start, &liter, v, BM_LOOPS_OF_VERT) {
				if ((l_start->f != ele_from) &&
				    FACE_WALK_TEST(l_start->f))
				{
					float dist_best = FLT_MAX;
					/* very similar to block above */
					state = state_step__face_edges(pc, state, &state_orig,
					                               l_start->next, l_start->prev, &dist_best);
					if (l_start->f->len > 3) {
						/* adjacent verts are handled in state_step__vert_edges */
						state = state_step__face_verts(pc, state, &state_orig,
						                               l_start->next->next, l_start->prev, &dist_best);
					}
				}
			}
		}

		/* vert edges  */
		{
			BMIter eiter;
			BMEdge *e;
			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				BMVert *v_other = BM_edge_other_vert(e, v);
				if (((BMElem *)e != ele_from) &&
				    VERT_WALK_TEST(v_other))
				{
					if (state_isect_co_exact(pc, v_other->co)) {
						BMElem *ele_next      = (BMElem *)v_other;
						BMElem *ele_next_from = (BMElem *)e;
						if (state_link_find(state, ele_next) == false) {
							if (state_orig.link_last != state->link_last) {
								state = state_dupe_add(pc, state, &state_orig);
							}
							state_link_add(pc, state, ele_next, ele_next_from);
						}
					}
				}
			}
		}
	}
	else {
		BLI_assert(0);
	}
	return (state_orig.link_last != state->link_last);
}

void bmo_connect_vert_pair_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *op_verts_slot = BMO_slot_get(op->slots_in, "verts");

	PathContext pc;
	bool found_all;
	float found_dist_best = -1.0f;

	if (op_verts_slot->len != 2) {
		/* fail! */
		return;
	}

	pc.bm_bmoflag = bm;
	pc.v_a = ((BMVert **)op_verts_slot->data.p)[0];
	pc.v_b = ((BMVert **)op_verts_slot->data.p)[1];

	/* fail! */
	if (!(pc.v_a && pc.v_b)) {
		return;
	}

#ifdef DEBUG_PRINT
	printf("%s: v_a: %d\n", __func__, BM_elem_index_get(pc.v_a));
	printf("%s: v_b: %d\n", __func__, BM_elem_index_get(pc.v_b));
#endif

	/* tag so we won't touch ever (typically hidden faces) */
	BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces_exclude", BM_FACE, FACE_EXCLUDE);
	BMO_slot_buffer_flag_enable(bm, op->slots_in, "verts_exclude", BM_VERT, VERT_EXCLUDE);

	/* setup context */
	{
		BLI_listbase_clear(&pc.state_lb);
		pc.link_pool = BLI_mempool_create(sizeof(PathLink), 0, 512, BLI_MEMPOOL_NOP);
	}

	/* calculate matrix */
	{
		float basis_dir[3];
		float basis_tmp[3];
		float basis_nor[3];


		sub_v3_v3v3(basis_dir, pc.v_a->co, pc.v_b->co);

#if 0
		add_v3_v3v3(basis_nor, pc.v_a->no, pc.v_b->no);
		cross_v3_v3v3(basis_tmp, basis_nor, basis_dir);
		cross_v3_v3v3(basis_nor, basis_tmp, basis_dir);
#else
		/* align both normals to the directions before combining */
		{
			float basis_nor_a[3];
			float basis_nor_b[3];

			/* align normal to direction */
			cross_v3_v3v3(basis_tmp,   pc.v_a->no, basis_dir);
			cross_v3_v3v3(basis_nor_a, basis_tmp,  basis_dir);

			cross_v3_v3v3(basis_tmp,   pc.v_b->no, basis_dir);
			cross_v3_v3v3(basis_nor_b, basis_tmp,  basis_dir);

			/* combine the normals */
			normalize_v3(basis_nor_a);
			normalize_v3(basis_nor_b);

			/* for flipped faces */
			if (dot_v3v3(basis_nor_a, basis_nor_b) < 0.0f) {
				negate_v3(basis_nor_b);
			}
			add_v3_v3v3(basis_nor, basis_nor_a, basis_nor_b);

			if (UNLIKELY(fabsf(dot_v3v3(basis_nor, basis_dir)) < FLT_EPSILON)) {
				ortho_v3_v3(basis_nor, basis_dir);
			}
		}
#endif

		/* get third axis */
		cross_v3_v3v3(basis_tmp, basis_dir, basis_nor);

		normalize_v3_v3(pc.matrix[0], basis_tmp);
		normalize_v3_v3(pc.matrix[1], basis_dir);
		normalize_v3_v3(pc.matrix[2], basis_nor);
		invert_m3(pc.matrix);

		pc.axis_sep = dot_m3_v3_row_x(pc.matrix, pc.v_a->co);
	}

	/* add first vertex */
	{
		PathLinkState *state;
		state = MEM_callocN(sizeof(*state), __func__);
		BLI_addtail(&pc.state_lb, state);
		state_link_add(&pc, state, (BMElem *)pc.v_a, NULL);
	}


	found_all = false;
	while (pc.state_lb.first) {
		PathLinkState *state, *state_next;
		found_all = true;
#ifdef DEBUG_PRINT
		printf("\n%s: stepping %d\n", __func__, BLI_countlist(&pc.state_lb));
#endif
		for (state = pc.state_lb.first; state; state = state_next) {
			state_next = state->next;
			if (state->link_last->ele == (BMElem *)pc.v_b) {
				/* pass, wait until all are found */
#ifdef DEBUG_PRINT
				printf("%s: state %p loop found %.4f\n", __func__, state, state->dist);
#endif
				if ((found_dist_best == -1.0f) || (found_dist_best > state->dist)) {
					found_dist_best = state->dist;
				}
			}
			else if (state_step(&pc, state)) {
				if ((found_dist_best != -1.0f) && (found_dist_best <= state->dist)) {
					BLI_remlink(&pc.state_lb, state);
					MEM_freeN(state);
				}
				found_all = false;
			}
			else {
				/* didn't reach the end, remove it,
				 * links are shared between states so just free the link_pool at the end */
				BLI_remlink(&pc.state_lb, state);
				MEM_freeN(state);
			}
		}

		if (found_all) {
#ifdef DEBUG
			for (state = pc.state_lb.first; state; state = state->next) {
				BLI_assert(state->link_last->ele == (BMElem *)pc.v_b);
			}
#endif
			break;
		}
	}

	if (BLI_listbase_is_empty(&pc.state_lb)) {
		found_all = false;
	}

	if (found_all) {
		PathLinkState *state, *state_best = NULL;
		PathLink *link;
		float state_best_dist = FLT_MAX;

		/* find the best state */
		for (state = pc.state_lb.first; state; state = state->next) {
			if ((state_best == NULL) || (state->dist < state_best_dist)) {
				state_best = state;
				state_best_dist = state_best->dist;
			}
		}

		link = state_best->link_last;
		do {
			if (link->ele->head.htype == BM_EDGE) {
				BMEdge *e = (BMEdge *)link->ele;
				BMVert *v_new;
				float e_fac = state_calc_co_pair_fac(&pc, e->v1->co, e->v2->co);
				v_new = BM_edge_split(bm, e, e->v1, NULL, e_fac);
				BMO_elem_flag_enable(bm, v_new, VERT_OUT);
			}
			else if (link->ele->head.htype == BM_VERT) {
				BMVert *v = (BMVert *)link->ele;
				BMO_elem_flag_enable(bm, v, VERT_OUT);
			}
			else {
				BLI_assert(0);
			}
		} while ((link = link->next));
	}

	BMO_elem_flag_enable(bm, pc.v_a, VERT_OUT);
	BMO_elem_flag_enable(bm, pc.v_b, VERT_OUT);

	BLI_mempool_destroy(pc.link_pool);
	BLI_freelistN(&pc.state_lb);

#if 1
	if (found_all) {
		BMOperator op_sub;
		BMO_op_initf(bm, &op_sub, 0,
		             "connect_verts verts=%fv faces_exclude=%s check_degenerate=%b",
		             VERT_OUT, op, "faces_exclude", true);
		BMO_op_exec(bm, &op_sub);
		BMO_slot_copy(&op_sub, slots_out, "edges.out",
		              op,      slots_out, "edges.out");
		BMO_op_finish(bm, &op_sub);
	}
#endif
}
