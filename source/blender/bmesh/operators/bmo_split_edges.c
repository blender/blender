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

/** \file blender/bmesh/operators/bmo_split_edges.c
 *  \ingroup bmesh
 *
 * Just a wrapper around #BM_mesh_edgesplit
 */

#include "BLI_utildefines.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h" /* own include */


/* keep this operator fast, its used in a modifier */
void bmo_split_edges_exec(BMesh *bm, BMOperator *op)
{
	const bool use_verts = BMO_slot_bool_get(op->slots_in, "use_verts");

	BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);
	BMO_slot_buffer_hflag_enable(bm, op->slots_in, "edges", BM_EDGE, BM_ELEM_TAG, false);

	if (use_verts) {
		/* this slows down the operation but its ok because the modifier doesn't use */
		BMO_slot_buffer_hflag_enable(bm, op->slots_in, "verts", BM_VERT, BM_ELEM_TAG, false);
	}

	/* this is where everything happens */
	BM_mesh_edgesplit(bm, use_verts, true, false);

	BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "edges.out", BM_EDGE, BM_ELEM_INTERNAL_TAG);
}
