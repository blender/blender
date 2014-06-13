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

/** \file blender/blenlib/intern/polyfill2d.c
 *  \ingroup bli
 *
 * A simple implementation of the ear cutting algorithm
 * to triangulate simple polygons without holes.
 *
 * \note
 *
 * Changes made for Blender.
 *
 * - loop the array to clip last verts first (less array resizing)
 *
 * - advance the ear to clip each iteration
 *   to avoid fan-filling convex shapes (USE_CLIP_EVEN).
 *
 * - avoid intersection tests when there are no convex points (USE_CONVEX_SKIP).
 *
 * \note
 *
 * No globals - keep threadsafe.
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLI_memarena.h"
#include "BLI_alloca.h"

#include "BLI_polyfill2d.h"  /* own include */

#include "BLI_strict_flags.h"

/* avoid fan-fill topology */
#define USE_CLIP_EVEN
#define USE_CONVEX_SKIP
/* sweep back-and-forth about convex ears (avoids lop-sided fans) */
#define USE_CLIP_SWEEP
// #define USE_CONVEX_SKIP_TEST

// #define DEBUG_TIME
#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
#endif


typedef signed char eSign;
enum {
	CONCAVE = -1,
	TANGENTIAL = 0,
	CONVEX = 1,
};

typedef struct PolyFill {
	struct PolyIndex *indices;  /* vertex aligned */

	const float (*coords)[2];
	unsigned int  coords_tot;
#ifdef USE_CONVEX_SKIP
	unsigned int  coords_tot_concave;
#endif

	/* A polygon with n vertices has a triangulation of n-2 triangles. */
	unsigned int (*tris)[3];
	unsigned int   tris_tot;
} PolyFill;


/* circular linklist */
typedef struct PolyIndex {
	struct PolyIndex *next, *prev;
	unsigned int index;
	eSign sign;
} PolyIndex;


/* based on libgdx 2013-11-28, apache 2.0 licensed */

static void pf_coord_sign_calc(PolyFill *pf, PolyIndex *pi);

static PolyIndex *pf_ear_tip_find(
        PolyFill *pf
#ifdef USE_CLIP_EVEN
        , PolyIndex *pi_ear_init
#endif
#ifdef USE_CLIP_SWEEP
        , bool reverse
#endif
        );

static bool       pf_ear_tip_check(PolyFill *pf, PolyIndex *pi_ear_tip);
static void       pf_ear_tip_cut(PolyFill *pf, PolyIndex *pi_ear_tip);


BLI_INLINE eSign signum_i(float a)
{
	if (UNLIKELY(a == 0.0f))
		return  0;
	else if (a > 0.0f)
		return  1;
	else
		return -1;
}

/**
 * alternative version of #area_tri_signed_v2
 * needed because of float precision issues
 *
 * \note removes / 2 since its not needed since we only need ths sign.
 */
BLI_INLINE float area_tri_signed_v2_alt_2x(const float v1[2], const float v2[2], const float v3[2])
{
	return ((v1[0] * (v2[1] - v3[1])) +
	        (v2[0] * (v3[1] - v1[1])) +
	        (v3[0] * (v1[1] - v2[1])));
}


static eSign span_tri_v2_sign(const float v1[2], const float v2[2], const float v3[2])
{
	return signum_i(area_tri_signed_v2_alt_2x(v3, v2, v1));
}

static unsigned int *pf_tri_add(PolyFill *pf)
{
	return pf->tris[pf->tris_tot++];
}

static void pf_coord_remove(PolyFill *pf, PolyIndex *pi)
{
	pi->next->prev = pi->prev;
	pi->prev->next = pi->next;

	if (UNLIKELY(pf->indices == pi)) {
		pf->indices = pi->next;
	}
#ifdef DEBUG
	pi->index = (unsigned int)-1;
	pi->next = pi->prev = NULL;
#endif

	pf->coords_tot -= 1;
}

static void pf_triangulate(PolyFill *pf)
{
	/* localize */
	PolyIndex *pi_ear;

#ifdef USE_CLIP_EVEN
	PolyIndex *pi_ear_init = pf->indices;
#endif
#ifdef USE_CLIP_SWEEP
	bool reverse = false;
#endif

	while (pf->coords_tot > 3) {
		PolyIndex *pi_prev, *pi_next;
		eSign sign_orig_prev, sign_orig_next;

		pi_ear = pf_ear_tip_find(
		        pf
#ifdef USE_CLIP_EVEN
		        , pi_ear_init
#endif
#ifdef USE_CLIP_SWEEP
		        , reverse
#endif
		        );

#ifdef USE_CLIP_SWEEP
#ifdef USE_CLIP_EVEN
		if (pi_ear != pi_ear_init) {
			reverse = !reverse;
		}
#else
		if (pi_ear != pf->indices) {
			reverse = !reverse;
		}
#endif
#endif

#ifdef USE_CONVEX_SKIP
		if (pi_ear->sign != CONVEX) {
			pf->coords_tot_concave -= 1;
		}
#endif

		pi_prev = pi_ear->prev;
		pi_next = pi_ear->next;

		pf_ear_tip_cut(pf, pi_ear);

		/* The type of the two vertices adjacent to the clipped vertex may have changed. */
		sign_orig_prev = pi_prev->sign;
		sign_orig_next = pi_next->sign;

		/* check if any verts became convex the (else if)
		 * case is highly unlikely but may happen with degenerate polygons */
		if (sign_orig_prev != CONVEX) {
			pf_coord_sign_calc(pf, pi_prev);
#ifdef USE_CONVEX_SKIP
			if (pi_prev->sign == CONVEX) {
				pf->coords_tot_concave -= 1;
			}
#endif
		}
		if (sign_orig_next != CONVEX) {
			pf_coord_sign_calc(pf, pi_next);
#ifdef USE_CONVEX_SKIP
			if (pi_next->sign == CONVEX) {
				pf->coords_tot_concave -= 1;
			}
#endif
		}

#ifdef USE_CLIP_EVEN
#ifdef USE_CLIP_SWEEP
		pi_ear_init = reverse ? pi_next->next : pi_prev->prev;
#else
		pi_ear_init = pi_next->next;
#endif
#endif

	}

	if (pf->coords_tot == 3) {
		unsigned int *tri = pf_tri_add(pf);
		pi_ear = pf->indices;
		tri[0] = pi_ear->index; pi_ear = pi_ear->next;
		tri[1] = pi_ear->index; pi_ear = pi_ear->next;
		tri[2] = pi_ear->index;
	}
}

/**
 * \return CONCAVE, TANGENTIAL or CONVEX
 */
static void pf_coord_sign_calc(PolyFill *pf, PolyIndex *pi)
{
	/* localize */
	const float (*coords)[2] = pf->coords;

	pi->sign = span_tri_v2_sign(
	        coords[pi->prev->index],
	        coords[pi->index],
	        coords[pi->next->index]);
}

static PolyIndex *pf_ear_tip_find(
        PolyFill *pf
#ifdef USE_CLIP_EVEN
        , PolyIndex *pi_ear_init
#endif
#ifdef USE_CLIP_SWEEP
        , bool reverse
#endif
        )
{
	/* localize */
	const unsigned int coords_tot = pf->coords_tot;
	PolyIndex *pi_ear;

	unsigned int i;

#ifdef USE_CLIP_EVEN
	pi_ear = pi_ear_init;
#else
	pi_ear = pf->indices;
#endif

	i = coords_tot;
	while (i--) {
		if (pf_ear_tip_check(pf, pi_ear)) {
			return pi_ear;
		}
#ifdef USE_CLIP_SWEEP
		pi_ear = reverse ? pi_ear->prev : pi_ear->next;
#else
		pi_ear = pi_ear->next;
#endif
	}

	/* Desperate mode: if no vertex is an ear tip, we are dealing with a degenerate polygon (e.g. nearly collinear).
	 * Note that the input was not necessarily degenerate, but we could have made it so by clipping some valid ears.
	 *
	 * Idea taken from Martin Held, "FIST: Fast industrial-strength triangulation of polygons", Algorithmica (1998),
	 * http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.115.291
	 *
	 * Return a convex or tangential vertex if one exists.
	 */

#ifdef USE_CLIP_EVEN
	pi_ear = pi_ear_init;
#else
	pi_ear = pf->indices;
#endif

	i = coords_tot;
	while (i--) {
		if (pi_ear->sign != CONCAVE) {
			return pi_ear;
		}
		pi_ear = pi_ear->next;
	}

	/* If all vertices are concave, just return the last one. */
	return pi_ear;
}

static bool pf_ear_tip_check(PolyFill *pf, PolyIndex *pi_ear_tip)
{
	/* localize */
	PolyIndex *pi_curr;
	const float (*coords)[2] = pf->coords;

	const float *v1, *v2, *v3;

#ifdef USE_CONVEX_SKIP
	unsigned int coords_tot_concave_checked = 0;
#endif


#ifdef USE_CONVEX_SKIP

#ifdef USE_CONVEX_SKIP_TEST
	/* check if counting is wrong */
	{
		unsigned int coords_tot_concave_test = 0;
		unsigned int i = pf->coords_tot;
		while (i--) {
			if (pf->indices[i].sign != CONVEX) {
				coords_tot_concave_test += 1;
			}
		}
		BLI_assert(coords_tot_concave_test == pf->coords_tot_concave);
	}
#endif

	/* fast-path for circles */
	if (pf->coords_tot_concave == 0) {
		return true;
	}
#endif

	if (UNLIKELY(pi_ear_tip->sign == CONCAVE)) {
		return false;
	}

	v1 = coords[pi_ear_tip->prev->index];
	v2 = coords[pi_ear_tip->index];
	v3 = coords[pi_ear_tip->next->index];

	/* Check if any point is inside the triangle formed by previous, current and next vertices.
	 * Only consider vertices that are not part of this triangle, or else we'll always find one inside. */

	for (pi_curr = pi_ear_tip->next->next; pi_curr != pi_ear_tip->prev; pi_curr = pi_curr->next) {
		/* Concave vertices can obviously be inside the candidate ear, but so can tangential vertices
		 * if they coincide with one of the triangle's vertices. */
		if (pi_curr->sign != CONVEX) {
			const float *v = coords[pi_curr->index];
			/* Because the polygon has clockwise winding order,
			 * the area sign will be positive if the point is strictly inside.
			 * It will be 0 on the edge, which we want to include as well. */

			/* note: check (v3, v1) first since it fails _far_ more often then the other 2 checks (those fail equally).
			 * It's logical - the chance is low that points exist on the same side as the ear we're clipping off. */
			if ((span_tri_v2_sign(v3, v1, v) != CONCAVE) &&
			    (span_tri_v2_sign(v1, v2, v) != CONCAVE) &&
			    (span_tri_v2_sign(v2, v3, v) != CONCAVE))
			{
				return false;
			}

#ifdef USE_CONVEX_SKIP
			coords_tot_concave_checked += 1;
			if (coords_tot_concave_checked == pf->coords_tot_concave) {
				break;
			}
#endif
		}
	}
	return true;
}

static void pf_ear_tip_cut(PolyFill *pf, PolyIndex *pi_ear_tip)
{
	unsigned int *tri = pf_tri_add(pf);

	tri[0] = pi_ear_tip->prev->index;
	tri[1] = pi_ear_tip->index;
	tri[2] = pi_ear_tip->next->index;

	pf_coord_remove(pf, pi_ear_tip);
}

/**
 * Triangulates the given (convex or concave) simple polygon to a list of triangle vertices.
 *
 * \param coords pairs describing vertices of the polygon, in either clockwise or counterclockwise order.
 * \return triples of triangle indices in clockwise order.
 *         Note the returned array is reused for later calls to the same method.
 */
static void polyfill_calc_ex(
        const float (*coords)[2],
        const unsigned int coords_tot,
        int coords_sign,
        unsigned int (*r_tris)[3],

        PolyIndex *r_indices)
{
	PolyFill pf;

	/* localize */
	PolyIndex *indices = r_indices;

	unsigned int i;

	/* assign all polyfill members here */
	pf.indices = r_indices;
	pf.coords = coords;
	pf.coords_tot = coords_tot;
#ifdef USE_CONVEX_SKIP
	pf.coords_tot_concave = 0;
#endif
	pf.tris = r_tris;
	pf.tris_tot = 0;

	if (coords_sign == 0) {
		coords_sign = (cross_poly_v2(coords, coords_tot) >= 0.0f) ? 1 : -1;
	}
	else {
		/* chech we're passing in correcty args */
#ifndef NDEBUG
		if (coords_sign == 1) {
			BLI_assert(cross_poly_v2(coords, coords_tot) >= 0.0f);
		}
		else {
			BLI_assert(cross_poly_v2(coords, coords_tot) <= 0.0f);
		}
#endif
	}

	if (coords_sign == 1) {
		for (i = 0; i < coords_tot; i++) {
			indices[i].next = &indices[i + 1];
			indices[i].prev = &indices[i - 1];
			indices[i].index = i;
		}
	}
	else {
		/* reversed */
		unsigned int n = coords_tot - 1;
		for (i = 0; i < coords_tot; i++) {
			indices[i].next = &indices[i + 1];
			indices[i].prev = &indices[i - 1];
			indices[i].index = (n - i);
		}
	}
	indices[0].prev = &indices[coords_tot - 1];
	indices[coords_tot - 1].next = &indices[0];

	for (i = 0; i < coords_tot; i++) {
		PolyIndex *pi = &indices[i];
		pf_coord_sign_calc(&pf, pi);
#ifdef USE_CONVEX_SKIP
		if (pi->sign != CONVEX) {
			pf.coords_tot_concave += 1;
		}
#endif
	}

	pf_triangulate(&pf);
}

void BLI_polyfill_calc_arena(
        const float (*coords)[2],
        const unsigned int coords_tot,
        const int coords_sign,
        unsigned int (*r_tris)[3],

        struct MemArena *arena)
{
	PolyIndex *indices = BLI_memarena_alloc(arena, sizeof(*indices) * coords_tot);

#ifdef DEBUG_TIME
	TIMEIT_START(polyfill2d);
#endif

	polyfill_calc_ex(
	        coords, coords_tot, coords_sign,
	        r_tris,
	        /* cache */

	        indices);

	/* indices are no longer needed,
	 * caller can clear arena */

#ifdef DEBUG_TIME
	TIMEIT_END(polyfill2d);
#endif
}

void BLI_polyfill_calc(
        const float (*coords)[2],
        const unsigned int coords_tot,
        const int coords_sign,
        unsigned int (*r_tris)[3])
{
	PolyIndex *indices = BLI_array_alloca(indices, coords_tot);

#ifdef DEBUG_TIME
	TIMEIT_START(polyfill2d);
#endif

	polyfill_calc_ex(
	        coords, coords_tot, coords_sign,
	        r_tris,
	        /* cache */

	        indices);

#ifdef DEBUG_TIME
	TIMEIT_END(polyfill2d);
#endif
}
