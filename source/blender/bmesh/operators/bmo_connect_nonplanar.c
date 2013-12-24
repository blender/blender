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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_connect_nonplanar.c
 *  \ingroup bmesh
 *
 * Connect verts non-planer faces iteratively (splits faces).
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_linklist_stack.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_OUT	(1 << 0)
#define FACE_OUT	(1 << 1)

/**
 * Calculates the face subset normal.
 */
static bool bm_face_subset_calc_normal(BMLoop *l_first, BMLoop *l_last, float r_no[3])
{
	float const *v_prev, *v_curr;

	/* Newell's Method */
	BMLoop *l_iter = l_first;
	BMLoop *l_term = l_last->next;

	zero_v3(r_no);

	v_prev = l_last->v->co;
	do {
		v_curr = l_iter->v->co;
		add_newell_cross_v3_v3v3(r_no, v_prev, v_curr);
		v_prev = v_curr;
	} while ((l_iter = l_iter->next) != l_term);

	return (normalize_v3(r_no) != 0.0f);
}

/**
 * Calculates how non-planar the face subset is.
 */
static float bm_face_subset_calc_planar(BMLoop *l_first, BMLoop *l_last, const float no[3])
{
	float axis_mat[3][3];
	float z_prev, z_curr;
	float delta_z = 0.0f;

	/* Newell's Method */
	BMLoop *l_iter = l_first;
	BMLoop *l_term = l_last->next;

	axis_dominant_v3_to_m3(axis_mat, no);

	z_prev = dot_m3_v3_row_z(axis_mat, l_last->v->co);
	do {
		z_curr = dot_m3_v3_row_z(axis_mat, l_iter->v->co);
		delta_z += fabsf(z_curr - z_prev);
		z_prev = z_curr;
	} while ((l_iter = l_iter->next) != l_term);

	return delta_z;
}

static bool bm_face_split_find(BMFace *f, BMLoop *l_pair[2], float *r_angle)
{
	BMLoop *l_iter, *l_first;
	BMLoop **l_arr = BLI_array_alloca(l_arr, f->len);
	const unsigned int f_len = f->len;
	unsigned int i_a, i_b;
	bool found = false;

	/* angle finding */
	float err_best = FLT_MAX;
	float angle_best = FLT_MAX;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	i_a = 0;
	do {
		l_arr[i_a++] = l_iter;
	} while ((l_iter = l_iter->next) != l_first);

	/* now for the big search, O(N^2), however faces normally aren't so large */
	for (i_a = 0; i_a < f_len; i_a++) {
		BMLoop *l_a = l_arr[i_a];
		for (i_b = i_a + 2; i_b < f_len; i_b++) {
			BMLoop *l_b = l_arr[i_b];
			/* check these are not touching
			 * (we could be smarter here) */
			if (!BM_loop_is_adjacent(l_a, l_b)) {
				/* first calculate normals */
				float no_a[3], no_b[3];

				if (bm_face_subset_calc_normal(l_a, l_b, no_a) &&
				    bm_face_subset_calc_normal(l_b, l_a, no_b))
				{
					const float err_a = bm_face_subset_calc_planar(l_a, l_b, no_a);
					const float err_b = bm_face_subset_calc_planar(l_b, l_a, no_b);
					const float err_test = err_a + err_b;

					if (err_test < err_best) {
						/* check we're legal (we could batch this) */
						BMLoop *l_split[2] = {l_a, l_b};
						BM_face_legal_splits(f, &l_split, 1);
						if (l_split[0]) {
							err_best = err_test;
							l_pair[0] = l_a;
							l_pair[1] = l_b;

							angle_best = angle_normalized_v3v3(no_a, no_b);
							found = true;
						}
					}
				}
			}
		}
	}

	*r_angle = angle_best;

	return found;


}

static bool bm_face_split_by_angle(BMesh *bm, BMFace *f, BMFace *r_f_pair[2], const float angle_limit)
{
	BMLoop *l_pair[2];
	float angle;

	if (bm_face_split_find(f, l_pair, &angle) && (angle > angle_limit)) {
		BMFace *f_new;
		BMLoop *l_new;

		f_new = BM_face_split(bm, f, l_pair[0], l_pair[1], &l_new, NULL, false);
		if (f_new) {
			r_f_pair[0] = f;
			r_f_pair[1] = f_new;

			BMO_elem_flag_enable(bm, f, FACE_OUT);
			BMO_elem_flag_enable(bm, f_new, FACE_OUT);
			BMO_elem_flag_enable(bm, l_new->e, EDGE_OUT);
			return true;
		}
	}

	return false;

}

void bmo_connect_verts_nonplanar_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *f;
	int totface = 0, totloop = 0;
	BLI_LINKSTACK_DECLARE(fstack, BMFace *);

	const float angle_limit = BMO_slot_float_get(op->slots_in, "angle_limit");


	BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
		if (f->len > 3) {
			totface += 1;
			totloop += f->len;
		}
	}

	if (totface == 0) {
		return;
	}

	BLI_LINKSTACK_INIT(fstack);

	BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
		if (f->len > 3) {
			BLI_LINKSTACK_PUSH(fstack, f);
		}
	}

	while ((f = BLI_LINKSTACK_POP(fstack))) {
		BMFace *f_pair[2];
		if (bm_face_split_by_angle(bm, f, f_pair, angle_limit)) {
			int j;
			for (j = 0; j < 2; j++) {
				BM_face_normal_update(f_pair[j]);
				if (f_pair[j]->len > 3) {
					BLI_LINKSTACK_PUSH(fstack, f_pair[j]);
				}
			}
		}
	}

	BLI_LINKSTACK_FREE(fstack);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT);
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
}
