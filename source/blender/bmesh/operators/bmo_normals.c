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

#define FACE_VIS	1
#define FACE_FLAG	2
#define FACE_FLIP	8

/*
 * put normal to the outside, and set the first direction flags in edges
 *
 * then check the object, and set directions / direction-flags: but only for edges with 1 or 2 faces
 * this is in fact the 'select connected'
 *
 * in case all faces were not done: start over with 'find the ultimate ...' */

/* NOTE: BM_ELEM_TAG is used on faces to tell if they are flipped. */

void bmo_recalc_face_normals_exec(BMesh *bm, BMOperator *op)
{
	BMFace **fstack;
	STACK_DECLARE(fstack);
	const bool use_face_tag = BMO_slot_bool_get(op->slots_in, "use_face_tag");
	const unsigned int tot_faces = BMO_slot_buffer_count(op->slots_in, "faces");
	unsigned int tot_touch = 0;

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces", BM_FACE, FACE_FLAG);

	fstack = MEM_mallocN(sizeof(*fstack) * tot_faces, __func__);

	while (tot_touch != tot_faces) {
		BMOIter siter;
		float f_len_best = -FLT_MAX;
		BMFace *f, *f_start = NULL;
		float f_start_cent[3];

		/* find a starting face */
		BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
			float f_cent[3];
			float f_len_test;

			/* clear dirty flag */
			BM_elem_flag_disable(f, BM_ELEM_TAG);

			if (BMO_elem_flag_test(bm, f, FACE_VIS))
				continue;

			if (!f_start) f_start = f;

			BM_face_calc_center_bounds(f, f_cent);

			if ((f_len_test = len_squared_v3(f_cent)) > f_len_best) {
				f_len_best = f_len_test;
				f_start = f;
				copy_v3_v3(f_start_cent, f_cent);
			}
		}

		/* check sanity (while loop ensures) */
		BLI_assert(f_start != NULL);

		/* make sure the starting face has the correct winding */
		if (dot_v3v3(f_start_cent, f_start->no) < 0.0f) {
			BM_face_normal_flip(bm, f_start);
			BMO_elem_flag_toggle(bm, f_start, FACE_FLIP);

			if (use_face_tag) {
				BM_elem_flag_toggle(f_start, BM_ELEM_TAG);
			}
		}

		/* now that we've found our starting face, make all connected faces
		 * have the same winding.  this is done recursively, using a manual
		 * stack (if we use simple function recursion, we'd end up overloading
		 * the stack on large meshes). */
		STACK_INIT(fstack);

		STACK_PUSH(fstack, f_start);
		BMO_elem_flag_enable(bm, f_start, FACE_VIS);
		tot_touch++;

		while ((f = STACK_POP(fstack))) {
			BMIter liter;
			BMLoop *l;

			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				BMLoop *l_other = l->radial_next;

				if ((l_other == l) || l_other->radial_next != l) {
					continue;
				}

				if (BMO_elem_flag_test(bm, l_other->f, FACE_FLAG)) {
					if (!BMO_elem_flag_test(bm, l_other->f, FACE_VIS)) {
						BMO_elem_flag_enable(bm, l_other->f, FACE_VIS);
						tot_touch++;


						if (l_other->v == l->v) {
							BM_face_normal_flip(bm, l_other->f);

							BMO_elem_flag_toggle(bm, l_other->f, FACE_FLIP);
							if (use_face_tag) {
								BM_elem_flag_toggle(l_other->f, BM_ELEM_TAG);
							}
						}
						else if (BM_elem_flag_test(l_other->f, BM_ELEM_TAG) || BM_elem_flag_test(l->f, BM_ELEM_TAG)) {
							if (use_face_tag) {
								BM_elem_flag_disable(l->f, BM_ELEM_TAG);
								BM_elem_flag_disable(l_other->f, BM_ELEM_TAG);
							}
						}

						STACK_PUSH(fstack, l_other->f);
					}
				}
			}
		}
	}

	MEM_freeN(fstack);
}
