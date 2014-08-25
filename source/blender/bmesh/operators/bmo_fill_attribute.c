
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

/** \file blender/bmesh/operators/bmo_fill_attribute.c
 *  \ingroup bmesh
 *
 * Fill in geometry with the attributes of their adjacent data.
 */

#include "BLI_utildefines.h"
#include "BLI_linklist_stack.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

/**
 * Check if all other loops are tagged.
 */
static bool bm_loop_is_all_radial_tag(BMLoop *l)
{
	BMLoop *l_iter;
	l_iter = l->radial_next;
	do {
		if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG) == 0) {
			return false;
		}
	} while ((l_iter = l_iter->radial_next) != l);

	return true;
}

/**
 * Callback to run on source-loops for #BM_face_copy_shared
 */
static bool bm_loop_is_face_untag(BMElem *ele, void *UNUSED(user_data))
{
	return (BM_elem_flag_test(((BMLoop *)ele)->f, BM_ELEM_TAG) == 0);
}

/**
 * Copy all attributes from adjacent untagged faces.
 */
static void bm_face_copy_shared_all(BMesh *bm, BMLoop *l,
                                    const bool use_normals, const bool use_data)
{
	BMLoop *l_other = l->radial_next;
	BMFace *f = l->f, *f_other;
	while (BM_elem_flag_test(l_other->f, BM_ELEM_TAG)) {
		l_other = l_other->radial_next;
	}
	f_other = l_other->f;

	if (use_data) {
		/* copy face-attrs */
		BM_elem_attrs_copy(bm, bm, f_other, f);

		/* copy loop-attrs */
		BM_face_copy_shared(bm, f, bm_loop_is_face_untag, NULL);
	}

	if (use_normals) {
		/* copy winding (flipping) */
		if (l->v == l_other->v) {
			BM_face_normal_flip(bm, f);
		}
	}
}

/**
 * Flood fill attributes.
 */
static unsigned int bmesh_face_attribute_fill(BMesh *bm,
                                              const bool use_normals, const bool use_data)
{
	BLI_LINKSTACK_DECLARE(loop_queue_prev, BMLoop *);
	BLI_LINKSTACK_DECLARE(loop_queue_next, BMLoop *);

	BMFace *f;
	BMIter iter;
	BMLoop *l;

	unsigned int face_tot = 0;


	BLI_LINKSTACK_INIT(loop_queue_prev);
	BLI_LINKSTACK_INIT(loop_queue_next);

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
			BMLoop *l_iter, *l_first;
			l_iter = l_first = BM_FACE_FIRST_LOOP(f);
			do {
				if (bm_loop_is_all_radial_tag(l_iter) == false) {
					BLI_LINKSTACK_PUSH(loop_queue_prev, l_iter);
				}
			} while ((l_iter = l_iter->next) != l_first);
		}
	}

	while (BLI_LINKSTACK_SIZE(loop_queue_prev)) {
		while ((l = BLI_LINKSTACK_POP(loop_queue_prev))) {
			/* check we're still un-assigned */
			if (BM_elem_flag_test(l->f, BM_ELEM_TAG)) {
				BMLoop *l_iter;

				BM_elem_flag_disable(l->f, BM_ELEM_TAG);

				l_iter = l->next;
				do {
					BMLoop *l_radial_iter = l_iter->radial_next;
					if (l_radial_iter != l_iter) {
						do {
							if (BM_elem_flag_test(l_radial_iter->f, BM_ELEM_TAG)) {
								BLI_LINKSTACK_PUSH(loop_queue_next, l_radial_iter);
							}
						} while ((l_radial_iter = l_radial_iter->radial_next) != l_iter);
					}
				} while ((l_iter = l_iter->next) != l);

				/* do last because of face flipping */
				bm_face_copy_shared_all(bm, l,
				                        use_normals, use_data);
				face_tot += 1;
			}
		}

		BLI_LINKSTACK_SWAP(loop_queue_prev, loop_queue_next);
	}

	BLI_LINKSTACK_FREE(loop_queue_prev);
	BLI_LINKSTACK_FREE(loop_queue_next);

	return face_tot;
}

void  bmo_face_attribute_fill_exec(BMesh *bm, BMOperator *op)
{
	const bool use_normals = BMO_slot_bool_get(op->slots_in, "use_normals");
	const bool use_data = BMO_slot_bool_get(op->slots_in, "use_data");

	int face_tot;

	BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);
	BMO_slot_buffer_hflag_enable(bm, op->slots_in, "faces", BM_FACE, BM_ELEM_TAG, false);  /* do inline */

	/* now we can copy adjacent data */
	face_tot = bmesh_face_attribute_fill(bm, use_normals, use_data);

	if (face_tot != BMO_slot_buffer_count(op->slots_in, "faces")) {
		/* any remaining tags will be skipped */
		BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "faces_fail.out", BM_FACE, BM_ELEM_TAG);
	}
}
