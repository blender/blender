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

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

enum {
	EDGE_SEAM  = 1
};

enum {
	VERT_SEAM  = 2
};

/**
 * Remove the EDGE_SEAM flag for edges we cant split
 *
 * un-tag edges not connected to other tagged edges,
 * unless they are on a boundary
 */
static void bm_edgesplit_validate_seams(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMEdge *e;

	unsigned char *vtouch;
	unsigned char *vt;

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	vtouch = MEM_callocN(sizeof(char) * bm->totvert, __func__);

	/* tag all boundary verts so as not to untag an edge which is inbetween only 2 faces [] */
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {

		/* unrelated to flag assignment in this function - since this is the
		 * only place we loop over all edges, disable tag */
		BM_elem_flag_disable(e, BM_ELEM_INTERNAL_TAG);

		if (BM_edge_is_boundary(e)) {
			vt = &vtouch[BM_elem_index_get(e->v1)]; if (*vt < 2) (*vt)++;
			vt = &vtouch[BM_elem_index_get(e->v2)]; if (*vt < 2) (*vt)++;

			/* while the boundary verts need to be tagged,
			 * the edge its self can't be split */
			BMO_elem_flag_disable(bm, e, EDGE_SEAM);
		}
	}

	/* single marked edges unconnected to any other marked edges
	 * are illegal, go through and unmark them */
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		/* lame, but we don't want the count to exceed 255,
		 * so just count to 2, its all we need */
		unsigned char *vt;
		vt = &vtouch[BM_elem_index_get(e->v1)]; if (*vt < 2) (*vt)++;
		vt = &vtouch[BM_elem_index_get(e->v2)]; if (*vt < 2) (*vt)++;
	}
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		if (vtouch[BM_elem_index_get(e->v1)] == 1 &&
		    vtouch[BM_elem_index_get(e->v2)] == 1)
		{
			BMO_elem_flag_disable(bm, e, EDGE_SEAM);
		}
	}

	MEM_freeN(vtouch);
}

/* keep this operator fast, its used in a modifier */
void bmo_edgesplit_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e;
	const int use_verts = BMO_slot_bool_get(op, "use_verts");

	BMO_slot_buffer_flag_enable(bm, op, "edges", BM_EDGE, EDGE_SEAM);

	if (use_verts) {
		/* this slows down the operation but its ok because the modifier doesn't use */
		BMO_slot_buffer_flag_enable(bm, op, "verts", BM_VERT, VERT_SEAM);

		/* prevent one edge having both verts unflagged
		 * we could alternately disable these edges, either way its a corner case.
		 *
		 * This is needed so we don't split off the edge but then none of its verts which
		 * would leave a duplicate edge.
		 */
		BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
			if (UNLIKELY((BMO_elem_flag_test(bm, e->v1, VERT_SEAM) == FALSE &&
			              (BMO_elem_flag_test(bm, e->v2, VERT_SEAM) == FALSE))))
			{
				BMO_elem_flag_enable(bm, e->v1, VERT_SEAM);
				BMO_elem_flag_enable(bm, e->v2, VERT_SEAM);
			}
		}
	}

	bm_edgesplit_validate_seams(bm, op);

	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		if (BMO_elem_flag_test(bm, e, EDGE_SEAM)) {
			/* this flag gets copied so we can be sure duplicate edges get it too (important) */
			BM_elem_flag_enable(e, BM_ELEM_INTERNAL_TAG);

			bmesh_edge_separate(bm, e, e->l);
			BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
			BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
		}
	}

	if (use_verts) {
		BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
			if (BMO_elem_flag_test(bm, e->v1, VERT_SEAM) == FALSE) {
				BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
			}
			if (BMO_elem_flag_test(bm, e->v2, VERT_SEAM) == FALSE) {
				BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
			}
		}
	}

	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		if (BMO_elem_flag_test(bm, e, EDGE_SEAM)) {
			if (BM_elem_flag_test(e->v1, BM_ELEM_TAG)) {
				BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
				bmesh_vert_separate(bm, e->v1, NULL, NULL);
			}
			if (BM_elem_flag_test(e->v2, BM_ELEM_TAG)) {
				BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
				bmesh_vert_separate(bm, e->v2, NULL, NULL);
			}
		}
	}

	BMO_slot_buffer_from_hflag(bm, op, "edgeout", BM_EDGE, BM_ELEM_INTERNAL_TAG);
}
