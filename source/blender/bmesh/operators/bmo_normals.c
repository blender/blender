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
#include "BLI_linklist_stack.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

/********* righthand faces implementation ****** */

#define FACE_FLAG	(1 << 0)
#define FACE_FLIP	(1 << 1)
#define FACE_TEMP	(1 << 2)

static bool bmo_recalc_normal_edge_filter_cb(BMElem *ele, void *UNUSED(user_data))
{
	return BM_edge_is_manifold((BMEdge *)ele);
}

/**
 * This uses a more comprehensive test to see if the furthest face from the center
 * is pointing towards the center or not.
 *
 * A simple test could just check the dot product of the faces-normal and the direction from the center,
 * however this can fail for faces which make a sharp spike. eg:
 *
 * <pre>
 * +
 * |\ <- face
 * + +
 *  \ \
 *   \ \
 *    \ +--------------+
 *     \               |
 *      \ center -> +  |
 *       \             |
 *        +------------+
 * </pre>
 *
 * In the example above, the a\ face can point towards the \a center
 * which would end up flipping the normals inwards.
 *
 * To take these spikes into account, use the normals of the faces edges.
 */
#define USE_FACE_EDGE_NORMAL_TEST

/**
 * The center of the entire island is't necessarily well placed,
 *
 * This re-calculated a center relative to this face.
 */
#ifdef USE_FACE_EDGE_NORMAL_TEST
#  define USE_FACE_LOCAL_CENTER_TEST
#endif

/**
 * \return a face index in \a faces and set \a r_is_flip if the face is flipped away from the center.
 */
static int recalc_face_normals_find_index(BMesh *bm, BMFace **faces, const int faces_len, bool *r_is_flip)
{
	float cent_area_accum = 0.0f;
	float f_len_best_sq;

	float cent[3], tvec[3];
	const float cent_fac = 1.0f / (float)faces_len;

	float (*faces_center)[3] = MEM_mallocN(sizeof(*faces_center) * faces_len, __func__);
	float *faces_area = MEM_mallocN(sizeof(*faces_area) * faces_len, __func__);
	bool is_flip = false;
	int f_start_index;
	int i;

	UNUSED_VARS_NDEBUG(bm);

	zero_v3(cent);

	/* first calculate the center */
	for (i = 0; i < faces_len; i++) {
		float *f_cent = faces_center[i];
		const float f_area = BM_face_calc_area(faces[i]);
		BM_face_calc_center_mean_weighted(faces[i], f_cent);
		madd_v3_v3fl(cent, f_cent, cent_fac * f_area);
		cent_area_accum += f_area;
		faces_area[i] = f_area;

		BLI_assert(BMO_elem_flag_test(bm, faces[i], FACE_TEMP) == 0);
		BLI_assert(BM_face_is_normal_valid(faces[i]));
	}

	if (cent_area_accum != 0.0f) {
		mul_v3_fl(cent, 1.0f / cent_area_accum);
	}

	f_len_best_sq = -FLT_MAX;
	/* used in degenerate cases only */
	f_start_index = 0;

	for (i = 0; i < faces_len; i++) {
		float f_len_test_sq;

		if (faces_area[i] > FLT_EPSILON) {
			if ((f_len_test_sq = len_squared_v3v3(faces_center[i], cent)) > f_len_best_sq) {
				f_len_best_sq = f_len_test_sq;
				f_start_index = i;
			}
		}
	}

#ifdef USE_FACE_EDGE_NORMAL_TEST
	{
		BMFace *f_test = faces[f_start_index];
		BMLoop *l_iter, *l_first;
		float e_len_best_sq = -FLT_MAX;
		BMLoop *l_other_best = NULL;
		float no_edge[3];
		const float *no_best;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f_test);
		do {
			if (BM_edge_is_manifold(l_iter->e) &&
			    bmo_recalc_normal_edge_filter_cb((BMElem *)l_iter->e, NULL))
			{
				BMLoop *l_other = l_iter->radial_next;

				if (len_squared_v3v3(l_iter->v->co, l_iter->next->v->co) > FLT_EPSILON) {
					float e_len_test_sq;
					float e_cent[3];
					mid_v3_v3v3(e_cent, l_iter->v->co, l_iter->next->v->co);
					e_len_test_sq = len_squared_v3v3(cent, e_cent);
					if (e_len_test_sq > e_len_best_sq) {
						l_other_best = l_other;
						e_len_best_sq = e_len_test_sq;
					}
				}
			}
		} while ((l_iter = l_iter->next) != l_first);

		/* furthest edge on furthest face */
		if (l_other_best) {
			float e_cent[3];

#ifdef USE_FACE_LOCAL_CENTER_TEST
			{
				float f_cent_other[3];
				BM_face_calc_center_mean_weighted(l_other_best->f, f_cent_other);
				mid_v3_v3v3(cent, f_cent_other, faces_center[f_start_index]);
			}
#endif
			mid_v3_v3v3(e_cent, l_other_best->e->v1->co, l_other_best->e->v2->co);
			sub_v3_v3v3(tvec, e_cent, cent);

			madd_v3_v3v3fl(no_edge, f_test->no, l_other_best->f->no, BM_edge_is_contiguous(l_other_best->e) ? 1 : -1);
			no_best = no_edge;
		}
		else {
			sub_v3_v3v3(tvec, faces_center[f_start_index], cent);
			no_best = f_test->no;
		}

		is_flip = (dot_v3v3(tvec, no_best) < 0.0f);
	}
#else
	sub_v3_v3v3(tvec, faces_center[f_start_index], cent);
	is_flip = (dot_v3v3(tvec, faces[f_start_index]->no) < 0.0f);
#endif

	/* make sure the starting face has the correct winding */
	MEM_freeN(faces_center);
	MEM_freeN(faces_area);

	*r_is_flip = is_flip;
	return f_start_index;
}

/**
 * Given an array of faces, recalculate their normals.
 * this functions assumes all faces in the array are connected by edges.
 *
 * \param bm
 * \param faces  Array of connected faces.
 * \param faces_len  Length of \a faces
 * \param oflag  Flag to check before doing the actual face flipping.
 */
static void bmo_recalc_face_normals_array(BMesh *bm, BMFace **faces, const int faces_len, const short oflag)
{
	int i, f_start_index;
	const short oflag_flip = oflag | FACE_FLIP;
	bool is_flip;

	BMFace *f;

	BLI_LINKSTACK_DECLARE(fstack, BMFace *);

	f_start_index = recalc_face_normals_find_index(bm, faces, faces_len, &is_flip);

	if (is_flip) {
		BMO_elem_flag_enable(bm, faces[f_start_index], FACE_FLIP);
	}

	/* now that we've found our starting face, make all connected faces
	 * have the same winding.  this is done recursively, using a manual
	 * stack (if we use simple function recursion, we'd end up overloading
	 * the stack on large meshes). */
	BLI_LINKSTACK_INIT(fstack);

	BLI_LINKSTACK_PUSH(fstack, faces[f_start_index]);
	BMO_elem_flag_enable(bm, faces[f_start_index], FACE_TEMP);

	while ((f = BLI_LINKSTACK_POP(fstack))) {
		const bool flip_state = BMO_elem_flag_test_bool(bm, f, FACE_FLIP);
		BMLoop *l_iter, *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BMLoop *l_other = l_iter->radial_next;

			if ((l_other != l_iter) && bmo_recalc_normal_edge_filter_cb((BMElem *)l_iter->e, NULL)) {
				if (!BMO_elem_flag_test(bm, l_other->f, FACE_TEMP)) {
					BMO_elem_flag_enable(bm, l_other->f, FACE_TEMP);
					BMO_elem_flag_set(bm, l_other->f, FACE_FLIP, (l_other->v == l_iter->v) != flip_state);
					BLI_LINKSTACK_PUSH(fstack, l_other->f);
				}
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	BLI_LINKSTACK_FREE(fstack);

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
	int *groups_array = MEM_mallocN(sizeof(*groups_array) * bm->totface, __func__);
	BMFace **faces_grp = MEM_mallocN(sizeof(*faces_grp) * bm->totface, __func__);

	int (*group_index)[2];
	const int group_tot = BM_mesh_calc_face_groups(bm, groups_array, &group_index,
	                                               bmo_recalc_normal_edge_filter_cb, NULL,
	                                               0, BM_EDGE);
	int i;

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces", BM_FACE, FACE_FLAG);

	BM_mesh_elem_table_ensure(bm, BM_FACE);

	for (i = 0; i < group_tot; i++) {
		const int fg_sta = group_index[i][0];
		const int fg_len = group_index[i][1];
		int j;
		bool is_calc = false;

		for (j = 0; j < fg_len; j++) {
			faces_grp[j] = BM_face_at_index(bm, groups_array[fg_sta + j]);

			if (is_calc == false) {
				is_calc = BMO_elem_flag_test_bool(bm, faces_grp[j], FACE_FLAG);
			}
		}

		if (is_calc) {
			bmo_recalc_face_normals_array(bm, faces_grp, fg_len, FACE_FLAG);
		}
	}

	MEM_freeN(faces_grp);

	MEM_freeN(groups_array);
	MEM_freeN(group_index);
}
