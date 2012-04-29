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

/** \file blender/bmesh/operators/bmo_slide.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BKE_global.h"

#include "BLI_math.h"

#include "bmesh.h"
#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_MARK 1
#define VERT_MARK 2

/*
 * Slides a vertex along a connected edge
 *
 */
void bmo_vertex_slide_exec(BMesh *bm, BMOperator *op)
{
	BMOIter oiter;
	BMIter iter;
	BMHeader *h;
	BMVert *vertex;
	BMEdge *edge;
	BMEdge *slide_edge;

	/* Selection counts */
	int selected_edges = 0;

	/* Get slide amount */
	const float distance_t = BMO_slot_float_get(op, "distance_t");

	/* Get start vertex */
	vertex = BMO_iter_new(&oiter, bm, op, "vert", BM_VERT);


	if (!vertex) {
		if (G.debug & G_DEBUG) {
			fprintf(stderr, "vertex_slide: No vertex selected...");
		}
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION, "Vertex Slide Error: Invalid selection.");
		return;
	}

	/* Count selected edges */
	BMO_ITER (h, &oiter, bm, op, "edge", BM_VERT | BM_EDGE) {
		switch (h->htype) {
			case BM_EDGE:
				selected_edges++;
				/* Mark all selected edges (cast BMHeader->BMEdge) */
				BMO_elem_flag_enable(bm, (BMElemF *)h, EDGE_MARK);
				break;
		}
	}

	/* Only allow sliding if an edge is selected */
	if (selected_edges == 0) {
		if (G.debug & G_DEBUG) {
			fprintf(stderr, "vertex_slide: select a single edge\n");
		}
		BMO_error_raise(bm, op, BMERR_INVALID_SELECTION, "Vertex Slide Error: Invalid selection.");
		return;
	}

	/* Make sure we get the correct edge. */
	BM_ITER_ELEM (edge, &iter, vertex, BM_EDGES_OF_VERT) {
		if (BMO_elem_flag_test(bm, edge, EDGE_MARK) && BM_vert_in_edge(edge, vertex)) {
			slide_edge = edge;
			break;
		}
	}

	/* Found edge */
	if (slide_edge) {
		BMVert *other = BM_edge_other_vert(slide_edge, vertex);

		/* mark */
		BMO_elem_flag_enable(bm, vertex, VERT_MARK);

		/* Interpolate */
		interp_v3_v3v3(vertex->co, vertex->co, other->co, distance_t);
	}

	/* Return the new edge. The same previously marked with VERT_MARK */
	BMO_slot_buffer_from_enabled_flag(bm, op, "vertout", BM_VERT, VERT_MARK);
	return;
}

#undef EDGE_MARK
#undef VERT_MARK
