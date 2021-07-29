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

/** \file blender/bmesh/operators/bmo_unsubdivide.c
 *  \ingroup bmesh
 *
 * Pattern based geometry reduction which has the result similar to undoing
 * a subdivide operation.
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h" /* own include */

/* - BMVert.flag & BM_ELEM_TAG:  shows we touched this vert
 * - BMVert.index == -1:         shows we will remove this vert
 */
void bmo_unsubdivide_exec(BMesh *bm, BMOperator *op)
{
	BMVert *v;
	BMIter iter;

	const int iterations = max_ii(1, BMO_slot_int_get(op->slots_in, "iterations"));

	BMOpSlot *vinput = BMO_slot_get(op->slots_in, "verts");
	BMVert **vinput_arr = (BMVert **)vinput->data.buf;
	int v_index;

	/* tag verts */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_disable(v, BM_ELEM_TAG);
	}
	for (v_index = 0; v_index < vinput->len; v_index++) {
		v = vinput_arr[v_index];
		BM_elem_flag_enable(v, BM_ELEM_TAG);
	}

	/* do all the real work here */
	BM_mesh_decimate_unsubdivide_ex(bm, iterations, true);
}
