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


/**
 * This may be useful outside the bmesh operator.
 *
 * \param v_grid  2d array of verts, all boundary verts must be set, we fill in the middle.
 */
static void bm_grid_fill_array(BMesh *bm, BMVert **v_grid, const int xtot, const int ytot,
                               const short mat_nr, const bool use_smooth,
                               const bool use_flip)
{
	const bool use_vert_interp = CustomData_has_interp(&bm->vdata);
	int x, y;

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
			{
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

#else
			interp_v3_v3v3(
			        co,
			        v_grid[x]->co,
			        v_grid[(xtot * ytot) + (x - xtot)]->co,
			        (float)y / ((float)ytot - 1));
#endif

			v = BM_vert_create(bm, co, NULL, 0);
			v_grid[(y * xtot) + x] = v;

			/* interpolate only along one axis, this could be changed
			 * but from user pov gives predictable results since these are selected loop */
			if (use_vert_interp) {
				void *v_cdata[2] = {
				    v_grid[XY(x,          0)]->head.data,
				    v_grid[XY(x, (ytot - 1))]->head.data,
				};
				const float t = (float)y / ((float)ytot - 1);
				const float w[2] = {1.0f - t, t};
				CustomData_bmesh_interp(&bm->vdata, v_cdata, w, NULL, 2, v->head.data);
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
				        false);
			}
			else {
				f = BM_face_create_quad_tri(
				        bm,
				        v_grid[XY(x + 1, y + 0)],  /* BR */
				        v_grid[XY(x + 1, y + 1)],  /* TR */
				        v_grid[XY(x,     y + 1)],  /* TL */
				        v_grid[XY(x,     y + 0)],  /* BL */
				        NULL,
				        false);
			}
			BMO_elem_flag_enable(bm, f, FACE_OUT);
			f->mat_nr = mat_nr;
			if (use_smooth) {
				BM_elem_flag_enable(f, BM_ELEM_SMOOTH);
			}
		}
	}
#undef XY
}

static void bm_grid_fill(BMesh *bm,
                         struct BMEdgeLoopStore *estore_a,      struct BMEdgeLoopStore *estore_b,
                         struct BMEdgeLoopStore *estore_rail_a, struct BMEdgeLoopStore *estore_rail_b,
                         const short mat_nr, const bool use_smooth)
{
#define USE_FLIP_DETECT

	const int xtot = BM_edgeloop_length_get(estore_a);
	const int ytot = BM_edgeloop_length_get(estore_rail_a);
	//BMVert *v;
	int i;
#ifdef DEBUG
	int x, y;
#endif
	LinkData *el;
	bool use_flip = false;

	ListBase *lb_a = BM_edgeloop_verts_get(estore_a);
	ListBase *lb_b = BM_edgeloop_verts_get(estore_b);

	ListBase *lb_rail_a = BM_edgeloop_verts_get(estore_rail_a);
	ListBase *lb_rail_b = BM_edgeloop_verts_get(estore_rail_b);

	BMVert **v_grid = MEM_callocN(sizeof(BMVert *) * xtot * ytot, __func__);
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


	bm_grid_fill_array(bm, v_grid, xtot, ytot, mat_nr, use_smooth, use_flip);
	MEM_freeN(v_grid);

#undef USE_FLIP_DETECT
}

static bool bm_edge_test_cb(BMEdge *e, void *bm_v)
{
	return BMO_elem_flag_test((BMesh *)bm_v, e, EDGE_MARK);
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
	const short mat_nr     = BMO_slot_int_get(op->slots_in,  "mat_nr");
	const bool use_smooth  = BMO_slot_bool_get(op->slots_in, "use_smooth");

	int count;
	bool change = false;
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

	BM_mesh_edgeloops_find_path(bm, &eloops_rail, bm_edge_test_rail_cb, (void *)bm, v_a_first, v_b_last);

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

		/* reverse so these so both are sorted the same way */
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
	             mat_nr, use_smooth);

	change = true;
cleanup:
	BM_mesh_edgeloops_free(&eloops);
	BM_mesh_edgeloops_free(&eloops_rail);

	if (change) {
		BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
	}
}
