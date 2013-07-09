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
 * Contributor(s): Joseph Eagar, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_normals.c
 *  \ingroup bmesh
 *
 * normal recalculation.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

/********* righthand faces implementation ****** */

#define FACE_FLAG	(1 << 0)
#define FACE_FLIP	(1 << 1)
#define FACE_TEMP	(1 << 2)

static bool bmo_recalc_normal_edge_filter_cb(BMEdge *e, void *UNUSED(user_data))
{
	return BM_edge_is_manifold(e);
}

/**
 * Given an array of faces, recalcualte their normals.
 * this functions assumes all faces in the array are connected by edges.
 *
 * \param bm
 * \param faces  Array of connected faces.
 * \param faces_len  Length of \a faces
 * \param oflag  Flag to check before doing the actual face flipping.
 */
static void bmo_recalc_face_normals_array(BMesh *bm, BMFace **faces, const int faces_len, const short oflag)
{
	float cent[3], tvec[3];
	float (*faces_center)[3] = MEM_mallocN(sizeof(*faces_center) * faces_len, __func__);
	const float cent_fac = 1.0f / (float)faces_len;
	int i, f_start_index;
	const short oflag_flip = oflag | FACE_FLIP;

	float f_len_best;
	BMFace *f;

	BMFace **fstack = MEM_mallocN(sizeof(*fstack) * faces_len, __func__);
	STACK_DECLARE(fstack);

	zero_v3(cent);

	/* first calculate the center */
	for (i = 0; i < faces_len; i++) {
		float *f_cent = faces_center[i];
		BM_face_calc_center_mean_weighted(faces[i], f_cent);
		madd_v3_v3fl(cent, f_cent, cent_fac);

		BLI_assert(BMO_elem_flag_test(bm, faces[i], FACE_TEMP) == 0);
	}

	f_len_best = -FLT_MAX;

	for (i = 0; i < faces_len; i++) {
		float f_len_test;

		if ((f_len_test = len_squared_v3v3(faces_center[i], cent)) > f_len_best) {
			f_len_best = f_len_test;
			f_start_index = i;
		}
	}

	/* make sure the starting face has the correct winding */
	sub_v3_v3v3(tvec, faces_center[f_start_index], cent);
	if (dot_v3v3(tvec, faces[f_start_index]->no) < 0.0f) {
		BMO_elem_flag_enable(bm, faces[f_start_index], FACE_FLIP);
	}

	MEM_freeN(faces_center);

	/* now that we've found our starting face, make all connected faces
	 * have the same winding.  this is done recursively, using a manual
	 * stack (if we use simple function recursion, we'd end up overloading
	 * the stack on large meshes). */
	STACK_INIT(fstack);

	STACK_PUSH(fstack, faces[f_start_index]);
	BMO_elem_flag_enable(bm, faces[f_start_index], FACE_TEMP);

	while ((f = STACK_POP(fstack))) {
		const bool flip_state = BMO_elem_flag_test_bool(bm, f, FACE_FLIP);
		BMLoop *l_iter, *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BMLoop *l_other = l_iter->radial_next;

			if ((l_other != l_iter) && bmo_recalc_normal_edge_filter_cb(l_iter->e, NULL)) {
				if (!BMO_elem_flag_test(bm, l_other->f, FACE_TEMP)) {
					BMO_elem_flag_enable(bm, l_other->f, FACE_TEMP);
					BMO_elem_flag_set(bm, l_other->f, FACE_FLIP, (l_other->v == l_iter->v) != flip_state);
					STACK_PUSH(fstack, l_other->f);
				}
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	MEM_freeN(fstack);

	/* apply flipping to oflag'd faces */
	for (i = 0; i < faces_len; i++) {
		if (BMO_elem_flag_test(bm, faces[i], oflag_flip) == oflag_flip) {
			BM_face_normal_flip(bm, faces[i]);
		}
		BMO_elem_flag_disable(bm, faces[i], FACE_TEMP);
	}
}

/*
 * put normal to the outside, and set the first direction flags in edges
 *
 * then check the object, and set directions / direction-flags: but only for edges with 1 or 2 faces
 * this is in fact the 'select connected'
 *
 * in case all faces were not done: start over with 'find the ultimate ...' */

void bmo_recalc_face_normals_exec(BMesh *bm, BMOperator *op)
{
	int *groups_array = MEM_mallocN(sizeof(groups_array) * bm->totface, __func__);
	int faces_len;
	BMFace **faces_arr = BM_iter_as_arrayN(bm, BM_FACES_OF_MESH, NULL, &faces_len, NULL, 0);
	BMFace **faces_grp = MEM_mallocN(sizeof(faces_grp) * bm->totface, __func__);

	int (*group_index)[2];
	const int group_tot = BM_mesh_calc_face_groups(bm, groups_array, &group_index,
	                                               NULL, bmo_recalc_normal_edge_filter_cb);
	int i;


	BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces", BM_FACE, FACE_FLAG);

	for (i = 0; i < group_tot; i++) {
		const int fg_sta = group_index[i][0];
		const int fg_len = group_index[i][1];
		int j;
		bool is_calc = false;

		for (j = 0; j < fg_len; j++) {
			faces_grp[j] = faces_arr[groups_array[fg_sta + j]];

			if (is_calc == false) {
				is_calc = BMO_elem_flag_test_bool(bm, faces_grp[j], FACE_FLAG);
			}
		}

		if (is_calc) {
			bmo_recalc_face_normals_array(bm, faces_grp, fg_len, FACE_FLAG);
		}
	}


	if (faces_arr) MEM_freeN(faces_arr);
	MEM_freeN(faces_grp);

	MEM_freeN(groups_array);
	MEM_freeN(group_index);
}
