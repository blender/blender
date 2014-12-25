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

/** \file blender/blenlib/intern/polyfill2d_beautify.c
 *  \ingroup bli
 *
 * This function is to improve the tessellation resulting from polyfill2d,
 * creating optimal topology.
 *
 * The functionality here matches #BM_mesh_beautify_fill,
 * but its far simpler to perform this operation in 2d,
 * on a simple polygon representation where we _know_:
 *
 * - The polygon is primitive with no holes with a continuous boundary.
 * - Tris have consistent winding.
 * - 2d (saves some hassles projecting face pairs on an axis for every edge-rotation)
 *   also saves us having to store all previous edge-states (see #EdRotState in bmesh_beautify.c)
 *
 * \note
 *
 * No globals - keep threadsafe.
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLI_memarena.h"
#include "BLI_edgehash.h"
#include "BLI_heap.h"

#include "BLI_polyfill2d_beautify.h"  /* own include */

#include "BLI_strict_flags.h"

struct PolyEdge {
	/** ordered vert indices (smaller first) */
	unsigned int verts[2];
	/** ordered face indices (depends on winding compared to the edge verts)
	 * - (verts[0], verts[1])  == faces[0]
	 * - (verts[1], verts[0])  == faces[1]
	 */
	unsigned int faces[2];
	/**
	 * The face-index which isn't used by either of the edges verts [0 - 2].
	 * could be calculated each time, but cleaner to store for reuse.
	 */
	unsigned int faces_other_v[2];
};


#ifndef NDEBUG
/**
 * Only to check for error-cases.
 */
static void polyfill_validate_tri(unsigned int (*tris)[3], unsigned int tri_index, EdgeHash *ehash)
{
	const unsigned int *tri = tris[tri_index];
	int j_curr;

	BLI_assert(!ELEM(tri[0], tri[1], tri[2]) &&
	           !ELEM(tri[1], tri[0], tri[2]) &&
	           !ELEM(tri[2], tri[0], tri[1]));

	for (j_curr = 0; j_curr < 3; j_curr++) {
		struct PolyEdge *e;
		unsigned int e_v1 = tri[(j_curr    )    ];
		unsigned int e_v2 = tri[(j_curr + 1) % 3];
		e = BLI_edgehash_lookup(ehash, e_v1, e_v2);
		if (e) {
			if (e->faces[0] == tri_index) {
				BLI_assert(e->verts[0] == e_v1);
				BLI_assert(e->verts[1] == e_v2);
			}
			else if (e->faces[1] == tri_index) {
				BLI_assert(e->verts[0] == e_v2);
				BLI_assert(e->verts[1] == e_v1);
			}
			else {
				BLI_assert(0);
			}

			BLI_assert(e->faces[0] != e->faces[1]);
			BLI_assert(ELEM(e_v1, UNPACK3(tri)));
			BLI_assert(ELEM(e_v2, UNPACK3(tri)));
			BLI_assert(ELEM(e_v1, UNPACK2(e->verts)));
			BLI_assert(ELEM(e_v2, UNPACK2(e->verts)));
			BLI_assert(e_v1 != tris[e->faces[0]][e->faces_other_v[0]]);
			BLI_assert(e_v1 != tris[e->faces[1]][e->faces_other_v[1]]);
			BLI_assert(e_v2 != tris[e->faces[0]][e->faces_other_v[0]]);
			BLI_assert(e_v2 != tris[e->faces[1]][e->faces_other_v[1]]);

			BLI_assert(ELEM(tri_index, UNPACK2(e->faces)));
		}
	}
}
#endif

BLI_INLINE bool is_boundary_edge(unsigned int i_a, unsigned int i_b, const unsigned int coord_last)
{
	BLI_assert(i_a < i_b);
	return ((i_a + 1 == i_b) || UNLIKELY((i_a == 0) && (i_b == coord_last)));
}
/**
 * Assuming we have 2 triangles sharing an edge (2 - 4),
 * check if the edge running from (1 - 3) gives better results.
 *
 * \return (negative number means the edge can be rotated, lager == better).
 */
static float quad_v2_rotate_beauty_calc(
        const float v1[2], const float v2[2], const float v3[2], const float v4[2])
{
	/* not a loop (only to be able to break out) */
	do {
		bool is_zero_a, is_zero_b;

		const float area_2x_234 = cross_tri_v2(v2, v3, v4);
		const float area_2x_241 = cross_tri_v2(v2, v4, v1);

		const float area_2x_123 = cross_tri_v2(v1, v2, v3);
		const float area_2x_134 = cross_tri_v2(v1, v3, v4);

		{
			BLI_assert((ELEM(v1, v2, v3, v4) == false) &&
			           (ELEM(v2, v1, v3, v4) == false) &&
			           (ELEM(v3, v1, v2, v4) == false) &&
			           (ELEM(v4, v1, v2, v3) == false));

			is_zero_a = (fabsf(area_2x_234) <= FLT_EPSILON);
			is_zero_b = (fabsf(area_2x_241) <= FLT_EPSILON);

			if (is_zero_a && is_zero_b) {
				break;
			}
		}

		if (is_zero_a == false && is_zero_b == false) {
			/* both tri's are valid, check we make a concave quad */
			if (!is_quad_convex_v2(v1, v2, v3, v4)) {
				break;
			}
		}
		else {
			/* one of the tri's was degenerate, chech we're not rotating
			 * into a different degenerate shape or flipping the face */
			if ((fabsf(area_2x_123) <= FLT_EPSILON) || (fabsf(area_2x_134) <= FLT_EPSILON)) {
				/* one of the new rotations is degenerate */
				break;
			}

			if ((area_2x_123 >= 0.0f) != (area_2x_134 >= 0.0f)) {
				/* rotation would cause flipping */
				break;
			}
		}

		{
			/* testing rule: the area divided by the perimeter,
			 * check if (1-3) beats the existing (2-4) edge rotation */
			float area_a, area_b;
			float prim_a, prim_b;
			float fac_24, fac_13;

			float len_12, len_23, len_34, len_41, len_24, len_13;

			/* edges around the quad */
			len_12 = len_v2v2(v1, v2);
			len_23 = len_v2v2(v2, v3);
			len_34 = len_v2v2(v3, v4);
			len_41 = len_v2v2(v4, v1);
			/* edges crossing the quad interior */
			len_13 = len_v2v2(v1, v3);
			len_24 = len_v2v2(v2, v4);

			/* note, area is in fact (area * 2),
			 * but in this case its OK, since we're comparing ratios */

			/* edge (2-4), current state */
			area_a = fabsf(area_2x_234);
			area_b = fabsf(area_2x_241);
			prim_a = len_23 + len_34 + len_24;
			prim_b = len_41 + len_12 + len_24;
			fac_24 = (area_a / prim_a) + (area_b / prim_b);

			/* edge (1-3), new state */
			area_a = fabsf(area_2x_123);
			area_b = fabsf(area_2x_134);
			prim_a = len_12 + len_23 + len_13;
			prim_b = len_34 + len_41 + len_13;
			fac_13 = (area_a / prim_a) + (area_b / prim_b);

			/* negative number if (1-3) is an improved state */
			return fac_24 - fac_13;
		}
	} while (false);

	return FLT_MAX;
}

static float polyedge_rotate_beauty_calc(
        const float (*coords)[2],
        const unsigned int (*tris)[3],
        const struct PolyEdge *e)
{
	const float *v1, *v2, *v3, *v4;

	v1 = coords[tris[e->faces[0]][e->faces_other_v[0]]];
	v3 = coords[tris[e->faces[1]][e->faces_other_v[1]]];
	v2 = coords[e->verts[0]];
	v4 = coords[e->verts[1]];

	return quad_v2_rotate_beauty_calc(v1, v2, v3, v4);
}

static void polyedge_beauty_cost_update_single(
        const float (*coords)[2],
        const unsigned int (*tris)[3],
        const struct PolyEdge *edges,
        struct PolyEdge *e,
        Heap *eheap, HeapNode **eheap_table)
{
	const unsigned int i = (unsigned int)(e - edges);

	if (eheap_table[i]) {
		BLI_heap_remove(eheap, eheap_table[i]);
		eheap_table[i] = NULL;
	}

	{
		/* recalculate edge */
		const float cost = polyedge_rotate_beauty_calc(coords, tris, e);
		if (cost < 0.0f) {
			eheap_table[i] = BLI_heap_insert(eheap, cost, e);
		}
		else {
			eheap_table[i] = NULL;
		}
	}
}

static void polyedge_beauty_cost_update(
        const float (*coords)[2],
        const unsigned int (*tris)[3],
        const struct PolyEdge *edges,
        struct PolyEdge *e,
        Heap *eheap, HeapNode **eheap_table,
        EdgeHash *ehash)
{
	const unsigned int *tri_0 = tris[e->faces[0]];
	const unsigned int *tri_1 = tris[e->faces[1]];
	unsigned int i;

	struct PolyEdge *e_arr[4] = {
		BLI_edgehash_lookup(ehash,
		        tri_0[(e->faces_other_v[0]    ) % 3],
		        tri_0[(e->faces_other_v[0] + 1) % 3]),
		BLI_edgehash_lookup(ehash,
		        tri_0[(e->faces_other_v[0] + 2) % 3],
		        tri_0[(e->faces_other_v[0]    ) % 3]),
		BLI_edgehash_lookup(ehash,
		        tri_1[(e->faces_other_v[1]    ) % 3],
		        tri_1[(e->faces_other_v[1] + 1) % 3]),
		BLI_edgehash_lookup(ehash,
		        tri_1[(e->faces_other_v[1] + 2) % 3],
		        tri_1[(e->faces_other_v[1]    ) % 3]),
	};


	for (i = 0; i < 4; i++) {
		if (e_arr[i]) {
			BLI_assert(!(ELEM(e_arr[i]->faces[0], UNPACK2(e->faces)) &&
			             ELEM(e_arr[i]->faces[1], UNPACK2(e->faces))));

			polyedge_beauty_cost_update_single(
			        coords, tris, edges,
			        e_arr[i],
			        eheap, eheap_table);
		}
	}
}

static void polyedge_rotate(
        unsigned int (*tris)[3],
        struct PolyEdge *e,
        EdgeHash *ehash)
{
	unsigned int e_v1_new = tris[e->faces[0]][e->faces_other_v[0]];
	unsigned int e_v2_new = tris[e->faces[1]][e->faces_other_v[1]];

#ifndef NDEBUG
	polyfill_validate_tri(tris, e->faces[0], ehash);
	polyfill_validate_tri(tris, e->faces[1], ehash);
#endif

	BLI_assert(e_v1_new != e_v2_new);
	BLI_assert(!ELEM(e_v2_new, UNPACK3(tris[e->faces[0]])));
	BLI_assert(!ELEM(e_v1_new, UNPACK3(tris[e->faces[1]])));

	tris[e->faces[0]][(e->faces_other_v[0] + 1) % 3] = e_v2_new;
	tris[e->faces[1]][(e->faces_other_v[1] + 1) % 3] = e_v1_new;

	e->faces_other_v[0] = (e->faces_other_v[0] + 2) % 3;
	e->faces_other_v[1] = (e->faces_other_v[1] + 2) % 3;

	BLI_assert((tris[e->faces[0]][e->faces_other_v[0]] != e_v1_new) &&
	           (tris[e->faces[0]][e->faces_other_v[0]] != e_v2_new));
	BLI_assert((tris[e->faces[1]][e->faces_other_v[1]] != e_v1_new) &&
	           (tris[e->faces[1]][e->faces_other_v[1]] != e_v2_new));

	BLI_edgehash_remove(ehash, e->verts[0], e->verts[1], NULL);
	BLI_edgehash_insert(ehash, e_v1_new, e_v2_new, e);

	if (e_v1_new < e_v2_new) {
		e->verts[0] = e_v1_new;
		e->verts[1] = e_v2_new;
	}
	else {
		/* maintain winding info */
		e->verts[0] = e_v2_new;
		e->verts[1] = e_v1_new;

		SWAP(unsigned int, e->faces[0], e->faces[1]);
		SWAP(unsigned int, e->faces_other_v[0], e->faces_other_v[1]);
	}

	/* update adjacent data */
	{
		unsigned int e_side = 0;

		for (e_side = 0; e_side < 2; e_side++) {
			/* 't_other' which we need to swap out is always the same edge-order */
			const unsigned int t_other = (((e->faces_other_v[e_side]) + 2)) % 3;
			unsigned int t_index = e->faces[e_side];
			unsigned int t_index_other = e->faces[!e_side];
			unsigned int *tri = tris[t_index];

			struct PolyEdge *e_other;
			unsigned int e_v1 = tri[(t_other    )    ];
			unsigned int e_v2 = tri[(t_other + 1) % 3];

			e_other = BLI_edgehash_lookup(ehash, e_v1, e_v2);
			if (e_other) {
				BLI_assert(t_index != e_other->faces[0] && t_index != e_other->faces[1]);
				if (t_index_other == e_other->faces[0]) {
					e_other->faces[0] = t_index;
					e_other->faces_other_v[0] = (t_other + 2) % 3;
					BLI_assert(!ELEM(tri[e_other->faces_other_v[0]], e_v1, e_v2));
				}
				else if (t_index_other == e_other->faces[1]) {
					e_other->faces[1] = t_index;
					e_other->faces_other_v[1] = (t_other + 2) % 3;
					BLI_assert(!ELEM(tri[e_other->faces_other_v[1]], e_v1, e_v2));
				}
				else {
					BLI_assert(0);
				}
			}
		}
	}

#ifndef NDEBUG
	polyfill_validate_tri(tris, e->faces[0], ehash);
	polyfill_validate_tri(tris, e->faces[1], ehash);
#endif

	BLI_assert(!ELEM(tris[e->faces[0]][e->faces_other_v[0]], UNPACK2(e->verts)));
	BLI_assert(!ELEM(tris[e->faces[1]][e->faces_other_v[1]], UNPACK2(e->verts)));
}

/**
 * The intention is that this calculates the output of #BLI_polyfill_calc
 *
 *
 * \note assumes the \a coords form a boundary,
 * so any edges running along contiguous (wrapped) indices,
 * are ignored since the edges wont share 2 faces.
 */
void BLI_polyfill_beautify(
        const float (*coords)[2],
        const unsigned int coords_tot,
        unsigned int (*tris)[3],

        /* structs for reuse */
        MemArena *arena, Heap *eheap, EdgeHash *ehash)
{
	const unsigned int coord_last = coords_tot - 1;
	const unsigned int tris_tot = coords_tot - 2;
	/* internal edges only (between 2 tris) */
	const unsigned int edges_tot = tris_tot - 1;
	unsigned int edges_tot_used = 0;
	unsigned int i;

	HeapNode **eheap_table;

	struct PolyEdge *edges = BLI_memarena_alloc(arena, edges_tot * sizeof(*edges));

	BLI_assert(BLI_heap_size(eheap) == 0);
	BLI_assert(BLI_edgehash_size(ehash) == 0);

	/* first build edges */
	for (i = 0; i < tris_tot; i++) {
		unsigned int j_prev, j_curr, j_next;
		j_prev = 2;
		j_next = 1;
		for (j_curr = 0; j_curr < 3; j_next = j_prev, j_prev = j_curr++) {
			int e_index;

			unsigned int e_pair[2] = {
				tris[i][j_prev],
				tris[i][j_curr],
			};

			if (e_pair[0] > e_pair[1]) {
				SWAP(unsigned int, e_pair[0], e_pair[1]);
				e_index = 1;
			}
			else {
				e_index = 0;
			}

			if (!is_boundary_edge(e_pair[0], e_pair[1], coord_last)) {
				struct PolyEdge *e = BLI_edgehash_lookup(ehash, e_pair[0], e_pair[1]);
				if (e == NULL) {
					e = &edges[edges_tot_used++];
					BLI_edgehash_insert(ehash, e_pair[0], e_pair[1], e);
					memcpy(e->verts, e_pair, sizeof(e->verts));
#ifndef NDEBUG
					e->faces[!e_index] = (unsigned int)-1;
#endif
				}
				else {

					/* ensure each edge only ever has 2x users */
#ifndef NDEBUG
					BLI_assert(e->faces[e_index] == (unsigned int)-1);
					BLI_assert((e->verts[0] == e_pair[0]) &&
					           (e->verts[1] == e_pair[1]));
#endif
				}

				e->faces[e_index] = i;
				e->faces_other_v[e_index] = j_next;
			}
		}
	}

	/* now perform iterative rotations */
	eheap_table = BLI_memarena_alloc(arena, sizeof(HeapNode *) * (size_t)edges_tot);

	// for (i = 0; i < tris_tot; i++) { polyfill_validate_tri(tris, i, eh); }

	/* build heap */
	for (i = 0; i < edges_tot; i++) {
		struct PolyEdge *e = &edges[i];
		const float cost = polyedge_rotate_beauty_calc(coords, (const unsigned int (*)[3])tris, e);
		if (cost < 0.0f) {
			eheap_table[i] = BLI_heap_insert(eheap, cost, e);
		}
		else {
			eheap_table[i] = NULL;
		}
	}

	while (BLI_heap_is_empty(eheap) == false) {
		struct PolyEdge *e = BLI_heap_popmin(eheap);
		i = (unsigned int)(e - edges);
		eheap_table[i] = NULL;

		polyedge_rotate(tris, e, ehash);

		/* recalculate faces connected on the heap */
		polyedge_beauty_cost_update(
		        coords, (const unsigned int (*)[3])tris, edges,
		        e,
		        eheap, eheap_table, ehash);
	}

	BLI_heap_clear(eheap, NULL);
	BLI_edgehash_clear_ex(ehash, NULL, BLI_POLYFILL_ALLOC_NGON_RESERVE);

	/* MEM_freeN(eheap_table); */  /* arena */
}
