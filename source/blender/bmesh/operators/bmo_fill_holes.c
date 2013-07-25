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

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_array.h"
#include "BLI_math.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_MARK	2
#define ELE_OUT		4

/**
 * Clone of BM_face_find_longest_loop that ensures the loop has an adjacent face
 */
static BMLoop *bm_face_find_longest_loop_manifold(BMFace *f)
{
	BMLoop *longest_loop = NULL;
	float longest_len = 0.0f;
	BMLoop *l_iter, *l_first;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);

	do {
		if (BM_edge_is_wire(l_iter->e) == false) {
			const float len = len_squared_v3v3(l_iter->v->co, l_iter->next->v->co);
			if (len >= longest_len) {
				longest_loop = l_iter;
				longest_len = len;
			}
		}
	} while ((l_iter = l_iter->next) != l_first);

	return longest_loop;
}

static BMFace *bm_face_from_eloop(BMesh *bm, struct BMEdgeLoopStore *el_store)
{
	LinkData *node = BM_edgeloop_verts_get(el_store)->first;
	const int len = BM_edgeloop_length_get(el_store);
	BMVert **f_verts = BLI_array_alloca(f_verts, len);
	BMFace *f;
	BMLoop *l;
	unsigned int i = 0;

	do {
		f_verts[i++] = node->data;
	} while ((node = node->next));

	f = BM_face_create_ngon_verts(bm, f_verts, len, 0, true, false);
	BM_face_copy_shared(bm, f);

	l = bm_face_find_longest_loop_manifold(f);
	if (l) {
		BMFace *f_other = l->radial_next->f;
		BLI_assert(l->radial_next != l);
		BM_elem_attrs_copy(bm, bm, f_other, f);
	}

	return f;
}

static bool bm_edge_test_cb(BMEdge *e, void *bm_v)
{
	return BMO_elem_flag_test((BMesh *)bm_v, e, EDGE_MARK);
}

void bmo_holes_fill_exec(BMesh *bm, BMOperator *op)
{
	ListBase eloops = {NULL, NULL};
	LinkData *el_store;

	BMEdge *e;
	int count;

	BMOIter siter;

	const int  sides    = BMO_slot_int_get(op->slots_in,  "sides");

	/* clear tags */

	BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

	/* tag edges that may be apart of loops */
	BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
		BMO_elem_flag_set(bm, e, EDGE_MARK, BM_edge_is_boundary(e));
	}

	count = BM_mesh_edgeloops_find(bm, &eloops, bm_edge_test_cb, (void *)bm);

	for (el_store = eloops.first; el_store; el_store = el_store->next) {
		if (BM_edgeloop_is_closed((struct BMEdgeLoopStore *)el_store)) {
			const int len = BM_edgeloop_length_get((struct BMEdgeLoopStore *)el_store);
			if ((sides == 0) || (len <= sides)) {
				BMFace *f;

				f = bm_face_from_eloop(bm, (struct BMEdgeLoopStore *)el_store);
				BMO_elem_flag_enable(bm, f, ELE_OUT);
			}
		}
	}

	(void)count;

	BM_mesh_edgeloops_free(&eloops);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, ELE_OUT);
}
