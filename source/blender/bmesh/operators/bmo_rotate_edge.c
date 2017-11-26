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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_rotate_edge.c
 *  \ingroup bmesh
 *
 * Rotate edges topology that share two faces.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_rotate_edges_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e, *e2;
	const bool use_ccw   = BMO_slot_bool_get(op->slots_in, "use_ccw");
	const bool is_single = BMO_slot_buffer_count(op->slots_in, "edges") == 1;
	short check_flag = is_single ?
	            BM_EDGEROT_CHECK_EXISTS :
	            BM_EDGEROT_CHECK_EXISTS | BM_EDGEROT_CHECK_DEGENERATE;

#define EDGE_OUT   1
#define FACE_TAINT 1

	BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
		/**
		 * this ends up being called twice, could add option to not to call check in
		 * #BM_edge_rotate to get some extra speed */
		if (BM_edge_rotate_check(e)) {
			BMFace *fa, *fb;
			if (BM_edge_face_pair(e, &fa, &fb)) {

				/* check we're untouched */
				if (BMO_face_flag_test(bm, fa, FACE_TAINT) == false &&
				    BMO_face_flag_test(bm, fb, FACE_TAINT) == false)
				{

					/* don't touch again (faces will be freed so run before rotating the edge) */
					BMO_face_flag_enable(bm, fa, FACE_TAINT);
					BMO_face_flag_enable(bm, fb, FACE_TAINT);

					if (!(e2 = BM_edge_rotate(bm, e, use_ccw, check_flag))) {

						BMO_face_flag_disable(bm, fa, FACE_TAINT);
						BMO_face_flag_disable(bm, fb, FACE_TAINT);
#if 0
						BMO_error_raise(bm, op, BMERR_INVALID_SELECTION, "Could not rotate edge");
						return;
#endif

						continue;
					}

					BMO_edge_flag_enable(bm, e2, EDGE_OUT);
				}
			}
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT);

#undef EDGE_OUT
#undef FACE_TAINT

}
