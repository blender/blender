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

/** \file blender/bmesh/operators/bmo_fill_holes.c
 *  \ingroup bmesh
 *
 * Fill boundary edge loop(s) with faces.
 */

#include "BLI_utildefines.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_holes_fill_exec(BMesh *bm, BMOperator *op)
{
	BMOperator op_attr;
	const unsigned int sides = BMO_slot_int_get(op->slots_in,  "sides");


	BM_mesh_elem_hflag_disable_all(bm, BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
	BMO_slot_buffer_hflag_enable(bm, op->slots_in, "edges", BM_EDGE, BM_ELEM_TAG, false);

	BM_mesh_edgenet(bm, true, true);  // TODO, sides


	/* bad - remove faces after as a workaround */
	if (sides != 0) {
		BMOIter siter;
		BMFace *f;

		BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "faces.out", BM_FACE, BM_ELEM_TAG);
		BMO_ITER (f, &siter, op->slots_out, "faces.out", BM_FACE) {
			if (f->len > sides) {
				BM_face_kill(bm, f);
			}
		}
	}

	BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "faces.out", BM_FACE, BM_ELEM_TAG);

	/* --- Attribute Fill --- */
	/* may as well since we have the faces already in a buffer */
	BMO_op_initf(bm, &op_attr, op->flag,
	             "face_attribute_fill faces=%S use_normals=%b use_data=%b",
	             op, "faces.out", true, true);

	BMO_op_exec(bm, &op_attr);

	/* check if some faces couldn't be touched */
	if (BMO_slot_buffer_count(op_attr.slots_out, "faces_fail.out")) {
		BMOIter siter;
		BMFace *f;

		BMO_ITER (f, &siter, op_attr.slots_out, "faces_fail.out", BM_FACE) {
			BM_face_normal_update(f);  /* normals are zero'd */
		}

		BMO_op_callf(bm, op->flag, "recalc_face_normals faces=%S", &op_attr, "faces_fail.out");
	}
	BMO_op_finish(bm, &op_attr);
}
