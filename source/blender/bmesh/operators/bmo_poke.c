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
 * Contributor(s): Francisco De La Cruz
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_poke.c
 *  \ingroup bmesh
 *
 * Pokes a face.
 *
 * Splits a face into a triangle fan.
 */

#include "BLI_math.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#include "BKE_customdata.h"

#define ELE_NEW 1

/**
 * Pokes a face
 *
 * Splits a face into a triangle fan.
 * Iterate over all selected faces, create a new center vertex and
 * create triangles between original face edges and new center vertex.
 */
void bmo_poke_exec(BMesh *bm, BMOperator *op)
{
	const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
	BMOIter oiter;
	BMFace *f;

	const float offset = BMO_slot_float_get(op->slots_in, "offset");
	const bool use_relative_offset = BMO_slot_bool_get(op->slots_in,  "use_relative_offset");
	const int center_mode = BMO_slot_int_get(op->slots_in, "center_mode");
	void (*bm_face_calc_center_fn)(const BMFace *f, float r_cent[3]);

	switch (center_mode) {
		case BMOP_POKE_MEDIAN_WEIGHTED:
			bm_face_calc_center_fn = BM_face_calc_center_median_weighted;
			break;
		case BMOP_POKE_BOUNDS:
			bm_face_calc_center_fn = BM_face_calc_center_bounds;
			break;
		case BMOP_POKE_MEDIAN:
			bm_face_calc_center_fn = BM_face_calc_center_median;
			break;
		default:
			BLI_assert(0);
			return;
	}

	BMO_ITER (f, &oiter, op->slots_in, "faces", BM_FACE) {
		BMFace *f_new;
		float f_center[3], f_center_mean[3];
		BMVert *v_center = NULL;
		BMLoop *l_iter, *l_first;
		/* only interpolate the central loop from the face once,
		 * then copy to all others in the fan */
		BMLoop *l_center_example;

		/* 1.0 or the average length from the center to the face verts */
		float offset_fac;

		int i;

		bm_face_calc_center_fn(f, f_center);
		v_center = BM_vert_create(bm, f_center, NULL, BM_CREATE_NOP);
		BMO_vert_flag_enable(bm, v_center, ELE_NEW);

		if (cd_loop_mdisp_offset != -1) {
			if (center_mode == BMOP_POKE_MEDIAN) {
				copy_v3_v3(f_center_mean, f_center);
			}
			else {
				BM_face_calc_center_median(f, f_center_mean);
			}
		}

		/* handled by BM_loop_interp_from_face */
		// BM_vert_interp_from_face(bm, v_center, f);

		if (use_relative_offset) {
			offset_fac = 0.0f;
		}
		else {
			offset_fac = 1.0f;
		}

		i = 0;
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BMLoop *l_new;

			f_new = BM_face_create_quad_tri(bm, l_iter->v, l_iter->next->v, v_center, NULL, f, BM_CREATE_NOP);
			l_new = BM_FACE_FIRST_LOOP(f_new);

			if (i == 0) {
				l_center_example = l_new->prev;
				BM_loop_interp_from_face(bm, l_center_example, f, true, false);
			}
			else {
				BM_elem_attrs_copy(bm, bm, l_center_example, l_new->prev);
			}

			/* Copy Loop Data */
			BM_elem_attrs_copy(bm, bm, l_iter, l_new);
			BM_elem_attrs_copy(bm, bm, l_iter->next, l_new->next);

			BMO_face_flag_enable(bm, f_new, ELE_NEW);

			if (cd_loop_mdisp_offset != -1) {
				float f_new_center[3];
				BM_face_calc_center_median(f_new, f_new_center);
				BM_face_interp_multires_ex(bm, f_new, f, f_new_center, f_center, cd_loop_mdisp_offset);
			}

			if (use_relative_offset) {
				offset_fac += len_v3v3(f_center, l_iter->v->co);
			}

		} while ((void)i++, (l_iter = l_iter->next) != l_first);

		if (use_relative_offset) {
			offset_fac /= (float)f->len;
		}
		/* else remain at 1.0 */

		copy_v3_v3(v_center->no, f->no);
		madd_v3_v3fl(v_center->co, v_center->no, offset * offset_fac);

		/* Kill Face */
		BM_face_kill(bm, f);
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, ELE_NEW);
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, ELE_NEW);
}
