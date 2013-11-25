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

/** \file blender/bmesh/operators/bmo_fill_grid.c
 *  \ingroup bmesh
 *
 * Fill 2 isolated, open edge loops with a grid of quads.
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#include "BLI_strict_flags.h"

#define EDGE_MARK	4
#define FACE_OUT	16

#define BARYCENTRIC_INTERP

#ifdef BARYCENTRIC_INTERP
/**
 * 2 edge vectors to normal.
 */
static void quad_edges_to_normal(
        float no[3],
        const float co_a1[3], const float co_a2[3],
        const float co_b1[3], const float co_b2[3])
{
	float diff_a[3];
	float diff_b[3];

	sub_v3_v3v3(diff_a, co_a2, co_a1);
	sub_v3_v3v3(diff_b, co_b2, co_b1);
	normalize_v3(diff_a);
	normalize_v3(diff_b);
	add_v3_v3v3(no, diff_a, diff_b);
	normalize_v3(no);
}

static void quad_verts_to_barycentric_tri(
        float tri[3][3],
        const float co_a[3],
        const float co_b[3],

        const float co_a_next[3],
        const float co_b_next[3],

        const float co_a_prev[3],
        const float co_b_prev[3],
        const bool is_flip
        )
{
	float no[3];

	copy_v3_v3(tri[0], co_a);
	copy_v3_v3(tri[1], co_b);

	quad_edges_to_normal(no,
	                     co_a, co_a_next,
	                     co_b, co_b_next);

	if (co_a_prev) {
		float no_t[3];
		quad_edges_to_normal(no_t,
		                     co_a_prev, co_a,
		                     co_b_prev, co_b);
		add_v3_v3(no, no_t);
		normalize_v3(no);
	}

	if (is_flip) negate_v3(no);
	mul_v3_fl(no, len_v3v3(tri[0], tri[1]));

	mid_v3_v3v3(tri[2], tri[0], tri[1]);
	add_v3_v3(tri[2], no);
}

#endif


/* -------------------------------------------------------------------- */
/* Handle Loop Pairs */

/** \name Loop Pairs
 * \{ */

/**
 * Assign a loop pair from 2 verts (which _must_ share an edge)
 */
static void bm_loop_pair_from_verts(BMVert *v_a, BMVert *v_b,
                                    BMLoop *l_pair[2])
{
	BMEdge *e = BM_edge_exists(v_a, v_b);
	if (e->l) {
		if (e->l->v == v_a) {
			l_pair[0] = e->l;
			l_pair[1] = e->l->next;
		}
		else {
			l_pair[0] = e->l->next;
			l_pair[1] = e->l;
		}
	}
	else {
		l_pair[0] = NULL;
		l_pair[1] = NULL;
	}
}

/**
 * Copy loop pair from one side to the other if either is missing,
 * this simplifies interpolation code so we only need to check if x/y are missing,
 * rather then checking each loop.
 */
static void bm_loop_pair_test_copy(BMLoop *l_pair_a[2], BMLoop *l_pair_b[2])
{
	/* if the first one is set, we know the second is too */
	if (l_pair_a[0] && l_pair_b[0] == NULL) {
		l_pair_b[0] = l_pair_a[1];
		l_pair_b[1] = l_pair_a[0];
	}
	else if (l_pair_b[0] && l_pair_a[0] == NULL) {
		l_pair_a[0] = l_pair_b[1];
		l_pair_a[1] = l_pair_b[0];
	}
}

/**
 * Interpolate from boundary loops.
 *
 * \note These weights will be calculated multiple times per vertex.
 */
static void bm_loop_interp_from_grid_boundary_4(BMesh *bm, BMLoop *l, BMLoop *l_bound[4], const float w[4])
{
	void *l_cdata[4] = {
	    l_bound[0]->head.data,
	    l_bound[1]->head.data,
	    l_bound[2]->head.data,
	    l_bound[3]->head.data};

	CustomData_bmesh_interp(&bm->ldata, l_cdata, w, NULL, 4, l->head.data);
}

static void bm_loop_interp_from_grid_boundary_2(BMesh *bm, BMLoop *l, BMLoop *l_bound[2], const float t)
{

	void *l_cdata[2] = {
	    l_bound[0]->head.data,
	    l_bound[1]->head.data};

	const float w[2] = {1.0f - t, t};

	CustomData_bmesh_interp(&bm->ldata, l_cdata, w, NULL, 2, l->head.data);
}

/** \} */


/**
 * Avoids calling #barycentric_weights_v2_quad often by caching weights into an array.
 */
static void barycentric_weights_v2_grid_cache(const unsigned int xtot, const unsigned int ytot,
                                              float (*weight_table)[4])
{
	float x_step = 1.0f / (float)(xtot - 1);
	float y_step = 1.0f / (float)(ytot - 1);
	unsigned int i = 0;
	float xy_fl[2];

	unsigned int x, y;
	for (y = 0; y < ytot; y++) {
		xy_fl[1] = y_step * (float)y;
		for (x = 0; x < xtot; x++) {
			xy_fl[0] = x_step * (float)x;
			{
				const float cos[4][2] = {
				    {xy_fl[0], 0.0f},
				    {0.0f, xy_fl[1]},
				    {xy_fl[0], 1.0f},
				    {1.0f, xy_fl[1]}};
				barycentric_weights_v2_quad(UNPACK4(cos), xy_fl, weight_table[i++]);
			}
		}
	}
}


/**
 * This may be useful outside the bmesh operator.
 *
 * \param v_grid  2d array of verts, all boundary verts must be set, we fill in the middle.
 */
static void bm_grid_fill_array(BMesh *bm, BMVert **v_grid, const unsigned int xtot, unsigned const int ytot,
                               const short mat_nr, const bool use_smooth,
                               const bool use_flip, const bool use_interp_simple)
{
	const bool use_vert_interp = CustomData_has_interp(&bm->vdata);
	const bool use_loop_interp = CustomData_has_interp(&bm->ldata);
	unsigned int x, y;

	/* for use_loop_interp */
	BMLoop *((*larr_x_a)[2]), *((*larr_x_b)[2]), *((*larr_y_a)[2]), *((*larr_y_b)[2]);

	float (*weight_table)[4];

#define XY(_x, _y)  ((_x) + ((_y) * (xtot)))

#ifdef BARYCENTRIC_INTERP
	float tri_a[3][3];
	float tri_b[3][3];
	float tri_t[3][3];  /* temp */

	quad_verts_to_barycentric_tri(
	        tri_a,
	        v_grid[XY(0,        0)]->co,
	        v_grid[XY(xtot - 1, 0)]->co,
	        v_grid[XY(0,        1)]->co,
	        v_grid[XY(xtot - 1, 1)]->co,
	        NULL, NULL,
	        false);

	quad_verts_to_barycentric_tri(
	        tri_b,
	        v_grid[XY(0,        (ytot - 1))]->co,
	        v_grid[XY(xtot - 1, (ytot - 1))]->co,
	        v_grid[XY(0,        (ytot - 2))]->co,
	        v_grid[XY(xtot - 1, (ytot - 2))]->co,
	        NULL, NULL,
	        true);
#endif

	if (use_interp_simple || use_vert_interp || use_loop_interp) {
		weight_table = MEM_mallocN(sizeof(*weight_table) * (size_t)(xtot * ytot), __func__);
		barycentric_weights_v2_grid_cache(xtot, ytot, weight_table);
	}
	else {
		weight_table = NULL;
	}


	/* Store loops */
	if (use_loop_interp) {
		/* x2 because each edge connects 2 loops */
		larr_x_a = MEM_mallocN(sizeof(*larr_x_a) * (xtot - 1), __func__);
		larr_x_b = MEM_mallocN(sizeof(*larr_x_b) * (xtot - 1), __func__);

		larr_y_a = MEM_mallocN(sizeof(*larr_y_a) * (ytot - 1), __func__);
		larr_y_b = MEM_mallocN(sizeof(*larr_y_b) * (ytot - 1), __func__);

		/* fill in the loops */
		for (x = 0; x < xtot - 1; x++) {
			bm_loop_pair_from_verts(v_grid[XY(x,        0)], v_grid[XY(x + 1,        0)], larr_x_a[x]);
			bm_loop_pair_from_verts(v_grid[XY(x, ytot - 1)], v_grid[XY(x + 1, ytot - 1)], larr_x_b[x]);
			bm_loop_pair_test_copy(larr_x_a[x], larr_x_b[x]);
		}

		for (y = 0; y < ytot - 1; y++) {
			bm_loop_pair_from_verts(v_grid[XY(0,        y)], v_grid[XY(0,        y + 1)], larr_y_a[y]);
			bm_loop_pair_from_verts(v_grid[XY(xtot - 1, y)], v_grid[XY(xtot - 1, y + 1)], larr_y_b[y]);
			bm_loop_pair_test_copy(larr_y_a[y], larr_y_b[y]);
		}
	}


	/* Build Verts */
	for (y = 1; y < ytot - 1; y++) {
#ifdef BARYCENTRIC_INTERP
		quad_verts_to_barycentric_tri(
		        tri_t,
		        v_grid[XY(0,        y + 0)]->co,
		        v_grid[XY(xtot - 1, y + 0)]->co,
		        v_grid[XY(0,        y + 1)]->co,
		        v_grid[XY(xtot - 1, y + 1)]->co,
		        v_grid[XY(0,        y - 1)]->co,
		        v_grid[XY(xtot - 1, y - 1)]->co,
		        false);
#endif
		for (x = 1; x < xtot - 1; x++) {
			float co[3];
			BMVert *v;
			/* we may want to allow sparse filled arrays, but for now, ensure its empty */
			BLI_assert(v_grid[(y * xtot) + x] == NULL);

			/* place the vertex */
#ifdef BARYCENTRIC_INTERP
			if (use_interp_simple == false) {
				float co_a[3], co_b[3];

				barycentric_transform(
				            co_a,
				            v_grid[x]->co,
				            tri_t[0], tri_t[1], tri_t[2],
				            tri_a[0], tri_a[1], tri_a[2]);
				barycentric_transform(
				            co_b,
				            v_grid[(xtot * ytot) + (x - xtot)]->co,
				            tri_t[0], tri_t[1], tri_t[2],
				            tri_b[0], tri_b[1], tri_b[2]);

				interp_v3_v3v3(co, co_a, co_b, (float)y / ((float)ytot - 1));
			}
			else
#endif
			{
				const float *w = weight_table[XY(x, y)];

				zero_v3(co);
				madd_v3_v3fl(co, v_grid[XY(x,        0)]->co, w[0]);
				madd_v3_v3fl(co, v_grid[XY(0,        y)]->co, w[1]);
				madd_v3_v3fl(co, v_grid[XY(x, ytot - 1)]->co, w[2]);
				madd_v3_v3fl(co, v_grid[XY(xtot - 1, y)]->co, w[3]);
			}

			v = BM_vert_create(bm, co, NULL, BM_CREATE_NOP);
			v_grid[(y * xtot) + x] = v;

			/* interpolate only along one axis, this could be changed
			 * but from user pov gives predictable results since these are selected loop */
			if (use_vert_interp) {
				const float *w = weight_table[XY(x, y)];

				void *v_cdata[4] = {
				    v_grid[XY(x,        0)]->head.data,
				    v_grid[XY(0,        y)]->head.data,
				    v_grid[XY(x, ytot - 1)]->head.data,
				    v_grid[XY(xtot - 1, y)]->head.data,
				};

				CustomData_bmesh_interp(&bm->vdata, v_cdata, w, NULL, 4, v->head.data);
			}

		}
	}

	/* Build Faces */
	for (x = 0; x < xtot - 1; x++) {
		for (y = 0; y < ytot - 1; y++) {
			BMFace *f;

			if (use_flip) {
				f = BM_face_create_quad_tri(
				        bm,
				        v_grid[XY(x,     y + 0)],  /* BL */
				        v_grid[XY(x,     y + 1)],  /* TL */
				        v_grid[XY(x + 1, y + 1)],  /* TR */
				        v_grid[XY(x + 1, y + 0)],  /* BR */
				        NULL,
				        BM_CREATE_NOP);
			}
			else {
				f = BM_face_create_quad_tri(
				        bm,
				        v_grid[XY(x + 1, y + 0)],  /* BR */
				        v_grid[XY(x + 1, y + 1)],  /* TR */
				        v_grid[XY(x,     y + 1)],  /* TL */
				        v_grid[XY(x,     y + 0)],  /* BL */
				        NULL,
				        BM_CREATE_NOP);
			}


			if (use_loop_interp && (larr_x_a[x][0] || larr_y_a[y][0])) {
				/* bottom/left/top/right */
				BMLoop *l_quad[4];
				BMLoop *l_bound[4];
				BMLoop *l_tmp;
				unsigned int x_side, y_side, i;
				char interp_from;


				if (larr_x_a[x][0] && larr_y_a[y][0]) {
					interp_from = 'B';  /* B == both */
					l_tmp = larr_x_a[x][0];
				}
				else if (larr_x_a[x][0]) {
					interp_from = 'X';
					l_tmp = larr_x_a[x][0];
				}
				else {
					interp_from = 'Y';
					l_tmp = larr_y_a[y][0];
				}

				BM_elem_attrs_copy(bm, bm, l_tmp->f, f);


				BM_face_as_array_loop_quad(f, l_quad);

				l_tmp = BM_FACE_FIRST_LOOP(f);

				if (use_flip) {
					l_quad[0] = l_tmp; l_tmp = l_tmp->next;
					l_quad[1] = l_tmp; l_tmp = l_tmp->next;
					l_quad[3] = l_tmp; l_tmp = l_tmp->next;
					l_quad[2] = l_tmp;
				}
				else {
					l_quad[2] = l_tmp; l_tmp = l_tmp->next;
					l_quad[3] = l_tmp; l_tmp = l_tmp->next;
					l_quad[1] = l_tmp; l_tmp = l_tmp->next;
					l_quad[0] = l_tmp;
				}

				i = 0;

				for (x_side = 0; x_side < 2; x_side++) {
					for (y_side = 0; y_side < 2; y_side++) {
						if (interp_from == 'B') {
							const float *w = weight_table[XY(x + x_side, y + y_side)];
							l_bound[0] = larr_x_a[x][x_side];  /* B */
							l_bound[1] = larr_y_a[y][y_side];  /* L */
							l_bound[2] = larr_x_b[x][x_side];  /* T */
							l_bound[3] = larr_y_b[y][y_side];  /* R */

							bm_loop_interp_from_grid_boundary_4(bm, l_quad[i++], l_bound, w);
						}
						else if (interp_from == 'X') {
							const float t = (float)(y + y_side) / (float)(ytot - 1);
							l_bound[0] = larr_x_a[x][x_side];  /* B */
							l_bound[1] = larr_x_b[x][x_side];  /* T */

							bm_loop_interp_from_grid_boundary_2(bm, l_quad[i++], l_bound, t);
						}
						else if (interp_from == 'Y') {
							const float t = (float)(x + x_side) / (float)(xtot - 1);
							l_bound[0] = larr_y_a[y][y_side];  /* L */
							l_bound[1] = larr_y_b[y][y_side];  /* R */

							bm_loop_interp_from_grid_boundary_2(bm, l_quad[i++], l_bound, t);
						}
						else {
							BLI_assert(0);
						}
					}
				}
			}
			/* end interp */


			BMO_elem_flag_enable(bm, f, FACE_OUT);
			f->mat_nr = mat_nr;
			if (use_smooth) {
				BM_elem_flag_enable(f, BM_ELEM_SMOOTH);
			}
		}
	}

	if (use_loop_interp) {
		MEM_freeN(larr_x_a);
		MEM_freeN(larr_y_a);
		MEM_freeN(larr_x_b);
		MEM_freeN(larr_y_b);
	}

	if (weight_table) {
		MEM_freeN(weight_table);
	}

#undef XY
}

static void bm_grid_fill(BMesh *bm,
                         struct BMEdgeLoopStore *estore_a,      struct BMEdgeLoopStore *estore_b,
                         struct BMEdgeLoopStore *estore_rail_a, struct BMEdgeLoopStore *estore_rail_b,
                         const short mat_nr, const bool use_smooth, const bool use_interp_simple)
{
#define USE_FLIP_DETECT

	const unsigned int xtot = (unsigned int)BM_edgeloop_length_get(estore_a);
	const unsigned int ytot = (unsigned int)BM_edgeloop_length_get(estore_rail_a);
	//BMVert *v;
	unsigned int i;
#ifdef DEBUG
	unsigned int x, y;
#endif
	LinkData *el;
	bool use_flip = false;

	ListBase *lb_a = BM_edgeloop_verts_get(estore_a);
	ListBase *lb_b = BM_edgeloop_verts_get(estore_b);

	ListBase *lb_rail_a = BM_edgeloop_verts_get(estore_rail_a);
	ListBase *lb_rail_b = BM_edgeloop_verts_get(estore_rail_b);

	BMVert **v_grid = MEM_callocN(sizeof(BMVert *) * (size_t)(xtot * ytot), __func__);
	/**
	 * <pre>
	 *           estore_b
	 *          +------------------+
	 *       ^  |                  |
	 *   end |  |                  |
	 *       |  |                  |
	 *       |  |estore_rail_a     |estore_rail_b
	 *       |  |                  |
	 * start |  |                  |
	 *          |estore_a          |
	 *          +------------------+
	 *                --->
	 *             start -> end
	 * </pre>
	 */

	BLI_assert(((LinkData *)lb_a->first)->data == ((LinkData *)lb_rail_a->first)->data);  /* BL */
	BLI_assert(((LinkData *)lb_b->first)->data == ((LinkData *)lb_rail_a->last)->data);   /* TL */
	BLI_assert(((LinkData *)lb_b->last)->data  == ((LinkData *)lb_rail_b->last)->data);   /* TR */
	BLI_assert(((LinkData *)lb_a->last)->data  == ((LinkData *)lb_rail_b->first)->data);  /* BR */

	for (el = lb_a->first,      i = 0; el; el = el->next, i++) { v_grid[i]                          = el->data; }
	for (el = lb_b->first,      i = 0; el; el = el->next, i++) { v_grid[(ytot * xtot) + (i - xtot)] = el->data; }
	for (el = lb_rail_a->first, i = 0; el; el = el->next, i++) { v_grid[xtot * i]                   = el->data; }
	for (el = lb_rail_b->first, i = 0; el; el = el->next, i++) { v_grid[(xtot * i) + (xtot - 1)]    = el->data; }
#ifdef DEBUG
	for (x = 1; x < xtot - 1; x++) { for (y = 1; y < ytot - 1; y++) { BLI_assert(v_grid[(y * xtot) + x] == NULL); }}
#endif

#ifdef USE_FLIP_DETECT
	{
		ListBase *lb_iter[4] = {lb_a, lb_b, lb_rail_a, lb_rail_b};
		const int lb_iter_dir[4] = {-1, 1, 1, -1};
		int winding_votes = 0;

		for (i = 0; i < 4; i++) {
			LinkData *el_next;
			for (el = lb_iter[i]->first; el && (el_next = el->next); el = el->next) {
				BMEdge *e = BM_edge_exists(el->data, el_next->data);
				if (BM_edge_is_boundary(e)) {
					winding_votes += (e->l->v == el->data) ? lb_iter_dir[i] : -lb_iter_dir[i];
				}
			}
		}
		use_flip = (winding_votes < 0);
	}
#endif


	bm_grid_fill_array(bm, v_grid, xtot, ytot, mat_nr, use_smooth, use_flip, use_interp_simple);
	MEM_freeN(v_grid);

#undef USE_FLIP_DETECT
}

static bool bm_edge_test_cb(BMEdge *e, void *bm_v)
{
	return BMO_elem_flag_test_bool((BMesh *)bm_v, e, EDGE_MARK);
}

static bool bm_edge_test_rail_cb(BMEdge *e, void *UNUSED(bm_v))
{
	/* normally operators dont check for hidden state
	 * but alternative would be to pass slot of rail edges */
	if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
		return false;
	}
	return BM_edge_is_wire(e) || BM_edge_is_boundary(e);
}

void bmo_grid_fill_exec(BMesh *bm, BMOperator *op)
{
	ListBase eloops = {NULL, NULL};
	ListBase eloops_rail = {NULL, NULL};
	struct BMEdgeLoopStore *estore_a, *estore_b;
	struct BMEdgeLoopStore *estore_rail_a, *estore_rail_b;
	BMVert *v_a_first, *v_a_last;
	BMVert *v_b_first, *v_b_last;
	const short mat_nr = (short)BMO_slot_int_get(op->slots_in,  "mat_nr");
	const bool use_smooth = BMO_slot_bool_get(op->slots_in, "use_smooth");
	const bool use_interp_simple = BMO_slot_bool_get(op->slots_in, "use_interp_simple");

	int count;
	bool changed = false;
	BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_MARK);

	count = BM_mesh_edgeloops_find(bm, &eloops, bm_edge_test_cb, (void *)bm);

	if (count != 2) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Select two edge loops");
		goto cleanup;
	}

	estore_a = eloops.first;
	estore_b = eloops.last;

	v_a_first = ((LinkData *)BM_edgeloop_verts_get(estore_a)->first)->data;
	v_a_last  = ((LinkData *)BM_edgeloop_verts_get(estore_a)->last)->data;
	v_b_first = ((LinkData *)BM_edgeloop_verts_get(estore_b)->first)->data;
	v_b_last  = ((LinkData *)BM_edgeloop_verts_get(estore_b)->last)->data;

	if (BM_edgeloop_length_get(estore_a) != BM_edgeloop_length_get(estore_b)) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Edge loop vertex count mismatch");
		goto cleanup;
	}

	if (BM_edgeloop_is_closed(estore_a) || BM_edgeloop_is_closed(estore_b)) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Closed loops unsupported");
		goto cleanup;
	}

	/* ok. all error checking done, now we can find the rail edges */

	if (BM_mesh_edgeloops_find_path(bm, &eloops_rail, bm_edge_test_rail_cb, bm, v_a_first, v_b_first) == false) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Loops are not connected by wire/boundary edges");
		goto cleanup;
	}

	/* We may find a first path, but not a second one! See geometry attached to bug [#37388]. */
	if (BM_mesh_edgeloops_find_path(bm, &eloops_rail, bm_edge_test_rail_cb, bm, v_a_first, v_b_last) == false) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Loops are not connected by wire/boundary edges");
		goto cleanup;
	}

	/* Check flipping by comparing path length */
	estore_rail_a = eloops_rail.first;
	estore_rail_b = eloops_rail.last;

	BLI_assert(BM_edgeloop_length_get(estore_rail_a) != BM_edgeloop_length_get(estore_rail_b));

	if (BM_edgeloop_length_get(estore_rail_a) < BM_edgeloop_length_get(estore_rail_b)) {
		BLI_remlink(&eloops_rail, estore_rail_b);
		BM_edgeloop_free(estore_rail_b);
		estore_rail_b = NULL;

		BM_mesh_edgeloops_find_path(bm, &eloops_rail, bm_edge_test_rail_cb, (void *)bm,
		                            v_a_last,
		                            v_b_last);
		estore_rail_b = eloops_rail.last;
	}
	else {  /* a > b */
		BLI_remlink(&eloops_rail, estore_rail_a);
		BM_edgeloop_free(estore_rail_a);
		estore_rail_a = estore_rail_b;

		/* reverse so both are sorted the same way */
		BM_edgeloop_flip(bm, estore_b);
		SWAP(BMVert *, v_b_first, v_b_last);

		BM_mesh_edgeloops_find_path(bm, &eloops_rail, bm_edge_test_rail_cb, (void *)bm,
		                            v_a_last,
		                            v_b_last);
		estore_rail_b = eloops_rail.last;
	}

	BLI_assert(estore_a != estore_b);
	BLI_assert(v_a_last != v_b_last);

	if (BM_edgeloop_length_get(estore_rail_a) != BM_edgeloop_length_get(estore_rail_b)) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Connecting edges vertex mismatch");
		goto cleanup;
	}

	if (BM_edgeloop_overlap_check(estore_rail_a, estore_rail_b)) {
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION,
		                "Connecting edge loops overlap");
		goto cleanup;
	}

	/* finally we have all edge loops needed */
	bm_grid_fill(bm, estore_a, estore_b, estore_rail_a, estore_rail_b,
	             mat_nr, use_smooth, use_interp_simple);

	changed = true;


cleanup:
	BM_mesh_edgeloops_free(&eloops);
	BM_mesh_edgeloops_free(&eloops_rail);

	if (changed) {
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
	}
}
