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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmesh_bevel.c
 *  \ingroup bmesh
 */

#include "BLI_utildefines.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_bevel_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMVert *v;

	const float offset = BMO_slot_float_get(op, "offset");
	const int seg = BMO_slot_int_get(op, "segments");

	if (offset > 0) {
		/* first flush 'geom' into flags, this makes it possible to check connected data */
		BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE, BM_ELEM_TAG, FALSE);

		BMO_ITER (v, &siter, bm, op, "geom", BM_VERT | BM_EDGE) {
			BM_elem_flag_enable(v, BM_ELEM_TAG);
		}

		BM_mesh_bevel(bm, offset, seg);
	}
}
