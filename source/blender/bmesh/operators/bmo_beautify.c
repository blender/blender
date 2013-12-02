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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_beautify.c
 *  \ingroup bmesh
 *
 * Beautify the mesh by rotating edges between triangles
 * to more attractive positions until no more rotations can be made.
 */

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"
#include "bmesh_tools.h"
#include "intern/bmesh_operators_private.h"

#define ELE_NEW		1
#define FACE_MARK	2

void bmo_beautify_fill_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter;
	BMOIter siter;
	BMFace *f;
	BMEdge *e;
	const bool use_restrict_tag = BMO_slot_bool_get(op->slots_in,  "use_restrict_tag");
	const short flag = (use_restrict_tag ? VERT_RESTRICT_TAG : 0);
	const short method = (short)BMO_slot_int_get(op->slots_in,  "method");

	BMEdge **edge_array;
	int edge_array_len = 0;
	BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
		if (f->len == 3) {
			BMO_elem_flag_enable(bm, f, FACE_MARK);
		}
	}

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_disable(e, BM_ELEM_TAG);
	}

	/* will over alloc if some edges can't be rotated */
	edge_array = MEM_mallocN(sizeof(*edge_array) * (size_t)BMO_slot_buffer_count(op->slots_in, "edges"), __func__);

	BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {

		/* edge is manifold and can be rotated */
		if (BM_edge_rotate_check(e) &&
		    /* faces are tagged */
		    BMO_elem_flag_test(bm, e->l->f, FACE_MARK) &&
		    BMO_elem_flag_test(bm, e->l->radial_next->f, FACE_MARK))
		{
			edge_array[edge_array_len] = e;
			edge_array_len++;
		}
	}

	BM_mesh_beautify_fill(bm, edge_array, edge_array_len, flag, method, ELE_NEW, FACE_MARK | ELE_NEW);

	MEM_freeN(edge_array);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_EDGE | BM_FACE, ELE_NEW);
}

