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
// #define USE_CONVEX_SKIP_TEST

typedef signed char eSign;
enum {
	CONCAVE = -1,
	TANGENTIAL = 0,
	CONVEX = 1,
};

typedef struct PolyFill {
	unsigned int *indices;  /* vertex aligned */

	const float (*coords)[2];
	unsigned int  coords_tot;
	eSign        *coords_sign;
#ifdef USE_CONVEX_SKIP
	unsigned int  coords_tot_concave;
#endif

	/* A polygon with n vertices has a triangulation of n-2 triangles. */
	unsigned int (*tris)[3];
	unsigned int   tris_tot;
} PolyFill;


/* based on libgdx 2013-11-28, apache 2.0 licensed */

static eSign pf_coord_sign_calc(PolyFill *pf, unsigned int index);
static unsigned int pf_index_prev(const PolyFill *pf, const unsigned int index);
static unsigned int pf_index_next(const PolyFill *pf, unsigned index);

#ifdef USE_CLIP_EVEN
static unsigned int pf_ear_tip_find(PolyFill *pf, const unsigned int index_offset);
#else
static unsigned int pf_ear_tip_find(PolyFill *pf);
#endif
static bool         pf_ear_tip_check(PolyFill *pf, const unsigned int index_ear_tip);
static void         pf_ear_tip_cut(PolyFill *pf, unsigned int index_ear_tip);


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

static void pf_coord_remove(PolyFill *pf, const unsigned int index)
{
	ARRAY_DELETE(pf->indices,     index, 1, pf->coords_tot);
	ARRAY_DELETE(pf->coords_sign, index, 1, pf->coords_tot);
	pf->coords_tot -= 1;
}

static void pf_triangulate(PolyFill *pf)
{
	/* localize */
	eSign *coords_sign = pf->coords_sign;

	unsigned int index_ear_tip = 0;


	while (pf->coords_tot > 3) {
		unsigned int i_prev, i_next;

#ifdef USE_CONVEX_SKIP
		eSign sign_orig_prev, sign_orig_next;
#endif


#ifdef USE_CLIP_EVEN
		index_ear_tip = pf_ear_tip_find(pf, index_ear_tip);
#else
		index_ear_tip = pf_ear_tip_find(pf);
#endif

#ifdef USE_CONVEX_SKIP
		if (coords_sign[index_ear_tip] != CONVEX) {
			pf->coords_tot_concave -= 1;
		}
#endif

		pf_ear_tip_cut(pf, index_ear_tip);

		/* The type of the two vertices adjacent to the clipped vertex may have changed. */
		i_prev = pf_index_prev(pf, index_ear_tip);
		i_next = (index_ear_tip == pf->coords_tot) ? 0 : index_ear_tip;

#ifdef USE_CONVEX_SKIP
		sign_orig_prev = coords_sign[i_prev];
		sign_orig_next = coords_sign[i_next];
#endif

		coords_sign[i_prev] = pf_coord_sign_calc(pf, i_prev);
		coords_sign[i_next] = pf_coord_sign_calc(pf, i_next);

#ifdef USE_CONVEX_SKIP
		/* check if any verts became convex the (else if)
		 * case is highly unlikely but may happen with degenerate polygons */
		if               ((sign_orig_prev != CONVEX) && (coords_sign[i_prev] == CONVEX))  pf->coords_tot_concave -= 1;
		else if (UNLIKELY((sign_orig_prev == CONVEX) && (coords_sign[i_prev] != CONVEX))) pf->coords_tot_concave += 1;

		if               ((sign_orig_next != CONVEX) && (coords_sign[i_next] == CONVEX))  pf->coords_tot_concave -= 1;
		else if (UNLIKELY((sign_orig_next == CONVEX) && (coords_sign[i_next] != CONVEX))) pf->coords_tot_concave += 1;
#endif

#ifdef USE_CLIP_EVEN
		index_ear_tip = i_prev;
#endif
	}

	if (pf->coords_tot == 3) {
		unsigned int *tri = pf_tri_add(pf);
		tri[0] = pf->indices[0];
		tri[1] = pf->indices[1];
		tri[2] = pf->indices[2];
	}
}

/**
 * \return CONCAVE, TANGENTIAL or CONVEX
 */
static eSign pf_coord_sign_calc(PolyFill *pf, unsigned int index)
{
	/* localize */
	unsigned int *indices = pf->indices;
	const float (*coords)[2] = pf->coords;

	return span_tri_v2_sign(
	        coords[indices[pf_index_prev(pf, index)]],
	        coords[indices[index]],
	        coords[indices[pf_index_next(pf, index)]]);
}

#ifdef USE_CLIP_EVEN
static unsigned int pf_ear_tip_find(PolyFill *pf, const unsigned int index_offset)
#else
static unsigned int pf_ear_tip_find(PolyFill *pf)
#endif
{
	/* localize */
	const eSign *coords_sign = pf->coords_sign;
	const unsigned int coords_tot = pf->coords_tot;

	unsigned int i;


	i = coords_tot;
	while (i--) {
#ifdef USE_CLIP_EVEN
		unsigned int j = (i + index_offset) % coords_tot;
		if (pf_ear_tip_check(pf, j)) {
			return j;
		}
#else
		if (pf_ear_tip_check(pf, i)) {
			return i;
		}
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

	i = coords_tot;
	while (i--) {
#ifdef USE_CLIP_EVEN
		unsigned int j = (i + index_offset) % coords_tot;
		if (coords_sign[j] != CONCAVE) {
			return j;
		}
#else
		if (coords_sign[i] != CONCAVE) {
			return i;
		}
#endif
	}

	/* If all vertices are concave, just return the last one. */
	return coords_tot - 1;
}

static bool pf_ear_tip_check(PolyFill *pf, const unsigned int index_ear_tip)
{
	/* localize */
	const unsigned int *indices = pf->indices;
	const float (*coords)[2] = pf->coords;
	const eSign *coords_sign = pf->coords_sign;

	unsigned int i_prev, i_curr, i_next;

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
			if (coords_sign[i] != CONVEX) {
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

	if (UNLIKELY(coords_sign[index_ear_tip] == CONCAVE)) {
		return false;
	}

	i_prev = pf_index_prev(pf, index_ear_tip);
	i_next = pf_index_next(pf, index_ear_tip);

	v1 = coords[indices[i_prev]];
	v2 = coords[indices[index_ear_tip]];
	v3 = coords[indices[i_next]];

	/* Check if any point is inside the triangle formed by previous, current and next vertices.
	 * Only consider vertices that are not part of this triangle, or else we'll always find one inside. */

	for (i_curr = pf_index_next(pf, i_next); i_curr != i_prev; i_curr = pf_index_next(pf, i_curr)) {
		/* Concave vertices can obviously be inside the candidate ear, but so can tangential vertices
		 * if they coincide with one of the triangle's vertices. */
		if (coords_sign[i_curr] != CONVEX) {
			const float *v = coords[indices[i_curr]];
			/* Because the polygon has clockwise winding order,
			 * the area sign will be positive if the point is strictly inside.
			 * It will be 0 on the edge, which we want to include as well. */
			if ((span_tri_v2_sign(v1, v2, v) != CONCAVE) &&
			    (span_tri_v2_sign(v2, v3, v) != CONCAVE) &&
			    (span_tri_v2_sign(v3, v1, v) != CONCAVE))
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

static void pf_ear_tip_cut(PolyFill *pf, unsigned int index_ear_tip)
{
	unsigned int *tri = pf_tri_add(pf);

	tri[0] = pf->indices[pf_index_prev(pf, index_ear_tip)];
	tri[1] = pf->indices[index_ear_tip];
	tri[2] = pf->indices[pf_index_next(pf, index_ear_tip)];

	pf_coord_remove(pf, index_ear_tip);
}

static unsigned int pf_index_prev(const PolyFill *pf, const unsigned int index)
{
	return (index ? index : pf->coords_tot) - 1;
}

static unsigned int pf_index_next(const PolyFill *pf, unsigned index)
{
	return (index + 1) % pf->coords_tot;
}

/**
* Triangulates the given (convex or concave) simple polygon to a list of triangle vertices.
* \param vertices pairs describing vertices of the polygon, in either clockwise or counterclockwise order.
* \return triples of triangle indices in clockwise order.
*         Note the returned array is reused for later calls to the same method.
*/
void BLI_polyfill_calc_ex(
        const float (*coords)[2],
        const unsigned int coords_tot,
        unsigned int (*r_tris)[3],

        unsigned int *r_indices, eSign *r_coords_sign)
{
	PolyFill pf;

	/* localize */
	unsigned int *indices = r_indices;
	eSign *coords_sign = r_coords_sign;

	unsigned int i;

	/* assign all polyfill members here */
	pf.indices = r_indices;
	pf.coords = coords;
	pf.coords_tot = coords_tot;
	pf.coords_sign = r_coords_sign;
#ifdef USE_CONVEX_SKIP
	pf.coords_tot_concave = 0;
#endif
	pf.tris = r_tris;
	pf.tris_tot = 0;

	if ((coords_tot < 3) ||
	    cross_poly_v2((int)coords_tot, (float(*)[2])coords) > 0.0f)
	{
		for (i = 0; i < coords_tot; i++) {
			indices[i] = i;
		}
	}
	else {
		/* reversed */
		unsigned int n = coords_tot - 1;
		for (i = 0; i < coords_tot; i++) {
			indices[i] = (n - i);
		}
	}

	for (i = 0; i < coords_tot; i++) {
		coords_sign[i] = pf_coord_sign_calc(&pf, i);
#ifdef USE_CONVEX_SKIP
		if (coords_sign[i] != CONVEX) {
			pf.coords_tot_concave += 1;
		}
#endif
	}

	pf_triangulate(&pf);
}

void BLI_polyfill_calc_arena(
        const float (*coords)[2],
        const unsigned int coords_tot,
        unsigned int (*r_tris)[3],

        struct MemArena *arena)
{
	unsigned int *indices = BLI_memarena_alloc(arena, sizeof(*indices) * coords_tot);
	eSign *coords_sign = BLI_memarena_alloc(arena, sizeof(*coords_sign) * coords_tot);

	BLI_polyfill_calc_ex(
	        coords, coords_tot,
	        r_tris,
	        /* cache */

	        indices, coords_sign);

	/* indices & coords_sign are no longer needed,
	 * caller can clear arena */
}

void BLI_polyfill_calc(
        const float (*coords)[2],
        const unsigned int coords_tot,
        unsigned int (*r_tris)[3])
{
	unsigned int *indices = BLI_array_alloca(indices, coords_tot);
	eSign *coords_sign = BLI_array_alloca(coords_sign, coords_tot);

	BLI_polyfill_calc_ex(
	        coords, coords_tot,
	        r_tris,
	        /* cache */

	        indices, coords_sign);
}
