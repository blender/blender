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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mesh_validate.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_sys_types.h"

#include "BLI_utildefines.h"
#include "BLI_edgehash.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"

#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"

#include "MEM_guardedalloc.h"

/* loop v/e are unsigned, so using max uint_32 value as invalid marker... */
#define INVALID_LOOP_EDGE_MARKER 4294967295u


/** \name Internal functions
 * \{ */

typedef union {
	uint32_t verts[2];
	int64_t edval;
} EdgeUUID;

typedef struct SortFace {
	EdgeUUID		es[4];
	unsigned int	index;
} SortFace;

/* Used to detect polys (faces) using exactly the same vertices. */
/* Used to detect loops used by no (disjoint) or more than one (intersect) polys. */
typedef struct SortPoly {
	int *verts;
	int numverts;
	int loopstart;
	unsigned int index;
	bool invalid;  /* Poly index. */
} SortPoly;

static void edge_store_assign(uint32_t verts[2],  const uint32_t v1, const uint32_t v2)
{
	if (v1 < v2) {
		verts[0] = v1;
		verts[1] = v2;
	}
	else {
		verts[0] = v2;
		verts[1] = v1;
	}
}

static void edge_store_from_mface_quad(EdgeUUID es[4], MFace *mf)
{
	edge_store_assign(es[0].verts, mf->v1, mf->v2);
	edge_store_assign(es[1].verts, mf->v2, mf->v3);
	edge_store_assign(es[2].verts, mf->v3, mf->v4);
	edge_store_assign(es[3].verts, mf->v4, mf->v1);
}

static void edge_store_from_mface_tri(EdgeUUID es[4], MFace *mf)
{
	edge_store_assign(es[0].verts, mf->v1, mf->v2);
	edge_store_assign(es[1].verts, mf->v2, mf->v3);
	edge_store_assign(es[2].verts, mf->v3, mf->v1);
	es[3].verts[0] = es[3].verts[1] = UINT_MAX;
}

static int int64_cmp(const void *v1, const void *v2)
{
	const int64_t x1 = *(const int64_t *)v1;
	const int64_t x2 = *(const int64_t *)v2;

	if (x1 > x2) {
		return 1;
	}
	else if (x1 < x2) {
		return -1;
	}

	return 0;
}

static int search_face_cmp(const void *v1, const void *v2)
{
	const SortFace *sfa = v1, *sfb = v2;

	if (sfa->es[0].edval > sfb->es[0].edval) {
		return 1;
	}
	else if	(sfa->es[0].edval < sfb->es[0].edval) {
		return -1;
	}

	else if	(sfa->es[1].edval > sfb->es[1].edval) {
		return 1;
	}
	else if	(sfa->es[1].edval < sfb->es[1].edval) {
		return -1;
	}

	else if	(sfa->es[2].edval > sfb->es[2].edval) {
		return 1;
	}
	else if	(sfa->es[2].edval < sfb->es[2].edval) {
		return -1;
	}

	else if	(sfa->es[3].edval > sfb->es[3].edval) {
		return 1;
	}
	else if	(sfa->es[3].edval < sfb->es[3].edval) {
		return -1;
	}

	return 0;
}

/* TODO check there is not some standard define of this somewhere! */
static int int_cmp(const void *v1, const void *v2)
{
	return *(int *)v1 > *(int *)v2 ? 1 : *(int *)v1 < *(int *)v2 ? -1 : 0;
}

static int search_poly_cmp(const void *v1, const void *v2)
{
	const SortPoly *sp1 = v1, *sp2 = v2;
	const int max_idx = sp1->numverts > sp2->numverts ? sp2->numverts : sp1->numverts;
	int idx;

	/* Reject all invalid polys at end of list! */
	if (sp1->invalid || sp2->invalid)
		return sp1->invalid ? (sp2->invalid ? 0 : 1) : -1;
	/* Else, sort on first non-equal verts (remember verts of valid polys are sorted). */
	for (idx = 0; idx < max_idx; idx++) {
		const int v1 = sp1->verts[idx];
		const int v2 = sp2->verts[idx];
		if (v1 != v2) {
			return (v1 > v2) ? 1 : -1;
		}
	}
	return sp1->numverts > sp2->numverts ? 1 : sp1->numverts < sp2->numverts ? -1 : 0;
}

static int search_polyloop_cmp(const void *v1, const void *v2)
{
	const SortPoly *sp1 = v1, *sp2 = v2;

	/* Reject all invalid polys at end of list! */
	if (sp1->invalid || sp2->invalid)
		return sp1->invalid && sp2->invalid ? 0 : sp1->invalid ? 1 : -1;
	/* Else, sort on loopstart. */
	return sp1->loopstart > sp2->loopstart ? 1 : sp1->loopstart < sp2->loopstart ? -1 : 0;
}
/** \} */



/* -------------------------------------------------------------------- */

/** \name Mesh Validation
 * \{ */

#define PRINT_MSG(...) (void) \
	( \
	 ((do_verbose) ? printf(__VA_ARGS__) : 0))

#define PRINT_ERR(...) (void) \
	(is_valid = false, \
	 ((do_verbose) ? printf(__VA_ARGS__) : 0))

/**
 * Validate the mesh, \a do_fixes requires \a mesh to be non-null.
 *
 * \return false if no changes needed to be made.
 */
bool BKE_mesh_validate_arrays(Mesh *mesh,
                              MVert *mverts, unsigned int totvert,
                              MEdge *medges, unsigned int totedge,
                              MFace *mfaces, unsigned int totface,
                              MLoop *mloops, unsigned int totloop,
                              MPoly *mpolys, unsigned int totpoly,
                              MDeformVert *dverts, /* assume totvert length */
                              const bool do_verbose, const bool do_fixes,
                              bool *r_changed)
{
#   define REMOVE_EDGE_TAG(_me) { _me->v2 = _me->v1; do_edge_free = true; } (void)0
#   define IS_REMOVED_EDGE(_me) (_me->v2 == _me->v1)

#   define REMOVE_LOOP_TAG(_ml) { _ml->e = INVALID_LOOP_EDGE_MARKER; do_polyloop_free = true; } (void)0
#   define REMOVE_POLY_TAG(_mp) { _mp->totloop *= -1; do_polyloop_free = true; } (void)0

	MVert *mv = mverts;
	MEdge *me;
	MLoop *ml;
	MPoly *mp;
	unsigned int i, j;
	int *v;

	bool is_valid = true;

	bool do_edge_free = false;
	bool do_face_free = false;
	bool do_polyloop_free = false; /* This regroups loops and polys! */

	bool verts_fixed = false;
	bool vert_weights_fixed = false;
	bool msel_fixed = false;

	bool do_edge_recalc = false;

	EdgeHash *edge_hash = BLI_edgehash_new_ex(__func__, totedge);

	BLI_assert(!(do_fixes && mesh == NULL));

	PRINT_MSG("%s: verts(%u), edges(%u), loops(%u), polygons(%u)\n",
	          __func__, totvert, totedge, totloop, totpoly);

	if (totedge == 0 && totpoly != 0) {
		PRINT_ERR("\tLogical error, %u polygons and 0 edges\n", totpoly);
		do_edge_recalc = do_fixes;
	}

	for (i = 0; i < totvert; i++, mv++) {
		bool fix_normal = true;

		for (j = 0; j < 3; j++) {
			if (!finite(mv->co[j])) {
				PRINT_ERR("\tVertex %u: has invalid coordinate\n", i);

				if (do_fixes) {
					zero_v3(mv->co);

					verts_fixed = true;
				}
			}

			if (mv->no[j] != 0)
				fix_normal = false;
		}

		if (fix_normal) {
			PRINT_ERR("\tVertex %u: has zero normal, assuming Z-up normal\n", i);
			if (do_fixes) {
				mv->no[2] = SHRT_MAX;
				verts_fixed = true;
			}
		}
	}

	for (i = 0, me = medges; i < totedge; i++, me++) {
		bool remove = false;
		if (me->v1 == me->v2) {
			PRINT_ERR("\tEdge %u: has matching verts, both %u\n", i, me->v1);
			remove = do_fixes;
		}
		if (me->v1 >= totvert) {
			PRINT_ERR("\tEdge %u: v1 index out of range, %u\n", i, me->v1);
			remove = do_fixes;
		}
		if (me->v2 >= totvert) {
			PRINT_ERR("\tEdge %u: v2 index out of range, %u\n", i, me->v2);
			remove = do_fixes;
		}

		if (BLI_edgehash_haskey(edge_hash, me->v1, me->v2)) {
			PRINT_ERR("\tEdge %u: is a duplicate of %d\n", i,
			          GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, me->v1, me->v2)));
			remove = do_fixes;
		}

		if (remove == false) {
			BLI_edgehash_insert(edge_hash, me->v1, me->v2, SET_INT_IN_POINTER(i));
		}
		else {
			REMOVE_EDGE_TAG(me);
		}
	}

	if (mfaces && !mpolys) {
#		define REMOVE_FACE_TAG(_mf) { _mf->v3 = 0; do_face_free = true; } (void)0
#		define CHECK_FACE_VERT_INDEX(a, b) \
					if (mf->a == mf->b) { \
						PRINT_ERR("    face %u: verts invalid, " STRINGIFY(a) "/" STRINGIFY(b) " both %u\n", i, mf->a); \
						remove = do_fixes; \
					} (void)0
#		define CHECK_FACE_EDGE(a, b) \
					if (!BLI_edgehash_haskey(edge_hash, mf->a, mf->b)) { \
						PRINT_ERR("    face %u: edge " STRINGIFY(a) "/" STRINGIFY(b) \
						          " (%u,%u) is missing edge data\n", i, mf->a, mf->b); \
						do_edge_recalc = true; \
					} (void)0

		MFace *mf;
		MFace *mf_prev;

		SortFace *sort_faces = MEM_callocN(sizeof(SortFace) * totface, "search faces");
		SortFace *sf;
		SortFace *sf_prev;
		unsigned int totsortface = 0;

		PRINT_ERR("No Polys, only tesselated Faces\n");

		for (i = 0, mf = mfaces, sf = sort_faces; i < totface; i++, mf++) {
			bool remove = false;
			int fidx;
			unsigned int fv[4];

			fidx = mf->v4 ? 3 : 2;
			do {
				fv[fidx] = *(&(mf->v1) + fidx);
				if (fv[fidx] >= totvert) {
					PRINT_ERR("\tFace %u: 'v%d' index out of range, %u\n", i, fidx + 1, fv[fidx]);
					remove = do_fixes;
				}
			} while (fidx--);

			if (remove == false) {
				if (mf->v4) {
					CHECK_FACE_VERT_INDEX(v1, v2);
					CHECK_FACE_VERT_INDEX(v1, v3);
					CHECK_FACE_VERT_INDEX(v1, v4);

					CHECK_FACE_VERT_INDEX(v2, v3);
					CHECK_FACE_VERT_INDEX(v2, v4);

					CHECK_FACE_VERT_INDEX(v3, v4);
				}
				else {
					CHECK_FACE_VERT_INDEX(v1, v2);
					CHECK_FACE_VERT_INDEX(v1, v3);

					CHECK_FACE_VERT_INDEX(v2, v3);
				}

				if (remove == false) {
					if (totedge) {
						if (mf->v4) {
							CHECK_FACE_EDGE(v1, v2);
							CHECK_FACE_EDGE(v2, v3);
							CHECK_FACE_EDGE(v3, v4);
							CHECK_FACE_EDGE(v4, v1);
						}
						else {
							CHECK_FACE_EDGE(v1, v2);
							CHECK_FACE_EDGE(v2, v3);
							CHECK_FACE_EDGE(v3, v1);
						}
					}

					sf->index = i;

					if (mf->v4) {
						edge_store_from_mface_quad(sf->es, mf);

						qsort(sf->es, 4, sizeof(int64_t), int64_cmp);
					}
					else {
						edge_store_from_mface_tri(sf->es, mf);
						qsort(sf->es, 3, sizeof(int64_t), int64_cmp);
					}

					totsortface++;
					sf++;
				}
			}

			if (remove) {
				REMOVE_FACE_TAG(mf);
			}
		}

		qsort(sort_faces, totsortface, sizeof(SortFace), search_face_cmp);

		sf = sort_faces;
		sf_prev = sf;
		sf++;

		for (i = 1; i < totsortface; i++, sf++) {
			bool remove = false;

			/* on a valid mesh, code below will never run */
			if (memcmp(sf->es, sf_prev->es, sizeof(sf_prev->es)) == 0) {
				mf = mfaces + sf->index;

				if (do_verbose) {
					mf_prev = mfaces + sf_prev->index;

					if (mf->v4) {
						PRINT_ERR("\tFace %u & %u: are duplicates (%u,%u,%u,%u) (%u,%u,%u,%u)\n",
						          sf->index, sf_prev->index, mf->v1, mf->v2, mf->v3, mf->v4,
						          mf_prev->v1, mf_prev->v2, mf_prev->v3, mf_prev->v4);
					}
					else {
						PRINT_ERR("\tFace %u & %u: are duplicates (%u,%u,%u) (%u,%u,%u)\n",
						          sf->index, sf_prev->index, mf->v1, mf->v2, mf->v3,
						          mf_prev->v1, mf_prev->v2, mf_prev->v3);
					}
				}

				remove = do_fixes;
			}
			else {
				sf_prev = sf;
			}

			if (remove) {
				REMOVE_FACE_TAG(mf);
			}
		}

		MEM_freeN(sort_faces);

#		undef REMOVE_FACE_TAG
#		undef CHECK_FACE_VERT_INDEX
#		undef CHECK_FACE_EDGE
	}

	/* Checking loops and polys is a bit tricky, as they are quite intricate...
	 *
	 * Polys must have:
	 * - a valid loopstart value.
	 * - a valid totloop value (>= 3 and loopstart+totloop < me.totloop).
	 *
	 * Loops must have:
	 * - a valid v value.
	 * - a valid e value (corresponding to the edge it defines with the next loop in poly).
	 *
	 * Also, loops not used by polys can be discarded.
	 * And "intersecting" loops (i.e. loops used by more than one poly) are invalid,
	 * so be sure to leave at most one poly per loop!
	 */
	{
		SortPoly *sort_polys = MEM_callocN(sizeof(SortPoly) * totpoly, "mesh validate's sort_polys");
		SortPoly *prev_sp, *sp = sort_polys;
		int prev_end;
		for (i = 0, mp = mpolys; i < totpoly; i++, mp++, sp++) {
			sp->index = i;

			if (mp->loopstart < 0 || mp->totloop < 3) {
				/* Invalid loop data. */
				PRINT_ERR("\tPoly %u is invalid (loopstart: %u, totloop: %u)\n",
				          sp->index, mp->loopstart, mp->totloop);
				sp->invalid = true;
			}
			else if (mp->loopstart + mp->totloop > totloop) {
				/* Invalid loop data. */
				PRINT_ERR("\tPoly %u uses loops out of range (loopstart: %u, loopend: %u, max nbr of loops: %u)\n",
				          sp->index, mp->loopstart, mp->loopstart + mp->totloop - 1, totloop - 1);
				sp->invalid = true;
			}
			else {
				/* Poly itself is valid, for now. */
				int v1, v2; /* v1 is prev loop vert idx, v2 is current loop one. */
				sp->invalid = false;
				sp->verts = v = MEM_mallocN(sizeof(int) * mp->totloop, "Vert idx of SortPoly");
				sp->numverts = mp->totloop;
				sp->loopstart = mp->loopstart;

				/* Test all poly's loops' vert idx. */
				for (j = 0, ml = &mloops[sp->loopstart]; j < mp->totloop; j++, ml++, v++) {
					if (ml->v >= totvert) {
						/* Invalid vert idx. */
						PRINT_ERR("\tLoop %u has invalid vert reference (%u)\n", sp->loopstart + j, ml->v);
						sp->invalid = true;
					}

					mverts[ml->v].flag |= ME_VERT_TMP_TAG;
					*v = ml->v;
				}

				/* is the same vertex used more than once */
				if (!sp->invalid) {
					v = sp->verts;
					for (j = 0; j < mp->totloop; j++, v++) {
						if ((mverts[*v].flag & ME_VERT_TMP_TAG) == 0) {
							PRINT_ERR("\tPoly %u has duplicate vert reference at corner (%u)\n", i, j);
							sp->invalid = true;
						}
						mverts[*v].flag &= ~ME_VERT_TMP_TAG;
					}
				}

				if (sp->invalid)
					continue;

				/* Test all poly's loops. */
				for (j = 0, ml = &mloops[sp->loopstart]; j < mp->totloop; j++, ml++) {
					v1 = ml->v;
					v2 = mloops[sp->loopstart + (j + 1) % mp->totloop].v;
					if (!BLI_edgehash_haskey(edge_hash, v1, v2)) {
						/* Edge not existing. */
						PRINT_ERR("\tPoly %u needs missing edge (%u, %u)\n", sp->index, v1, v2);
						if (do_fixes)
							do_edge_recalc = true;
						else
							sp->invalid = true;
					}
					else if (ml->e >= totedge) {
						/* Invalid edge idx.
						 * We already know from previous text that a valid edge exists, use it (if allowed)! */
						if (do_fixes) {
							int prev_e = ml->e;
							ml->e = GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, v1, v2));
							PRINT_ERR("\tLoop %u has invalid edge reference (%u), fixed using edge %u\n",
							          sp->loopstart + j, prev_e, ml->e);
						}
						else {
							PRINT_ERR("\tLoop %u has invalid edge reference (%u)\n", sp->loopstart + j, ml->e);
							sp->invalid = true;
						}
					}
					else {
						me = &medges[ml->e];
						if (IS_REMOVED_EDGE(me) || !((me->v1 == v1 && me->v2 == v2) || (me->v1 == v2 && me->v2 == v1))) {
							/* The pointed edge is invalid (tagged as removed, or vert idx mismatch),
							 * and we already know from previous test that a valid one exists, use it (if allowed)! */
							if (do_fixes) {
								int prev_e = ml->e;
								ml->e = GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, v1, v2));
								PRINT_ERR("\tPoly %u has invalid edge reference (%u), fixed using edge %u\n",
								          sp->index, prev_e, ml->e);
							}
							else {
								PRINT_ERR("\tPoly %u has invalid edge reference (%u)\n", sp->index, ml->e);
								sp->invalid = true;
							}
						}
					}
				}

				/* Now check that that poly does not use a same vertex more than once! */
				if (!sp->invalid) {
					const int *prev_v = v = sp->verts;
					j = sp->numverts;

					qsort(sp->verts, j, sizeof(int), int_cmp);

					for (j--, v++; j; j--, v++) {
						if (*v != *prev_v) {
							int dlt = v - prev_v;
							if (dlt > 1) {
								PRINT_ERR("\tPoly %u is invalid, it multi-uses vertex %u (%u times)\n",
								          sp->index, *prev_v, dlt);
								sp->invalid = true;
							}
							prev_v = v;
						}
					}
					if (v - prev_v > 1) { /* Don't forget final verts! */
						PRINT_ERR("\tPoly %u is invalid, it multi-uses vertex %u (%u times)\n",
						          sp->index, *prev_v, (int)(v - prev_v));
						sp->invalid = true;
					}
				}
			
			}
		}

		/* Second check pass, testing polys using the same verts. */
		qsort(sort_polys, totpoly, sizeof(SortPoly), search_poly_cmp);
		sp = prev_sp = sort_polys;
		sp++;

		for (i = 1; i < totpoly; i++, sp++) {
			int p1_nv = sp->numverts, p2_nv = prev_sp->numverts;
			const int *p1_v = sp->verts, *p2_v = prev_sp->verts;
			short p1_sub = true, p2_sub = true;
			if (sp->invalid)
				break;
			/* Test same polys. */
#if 0
			/* NOTE: This performs a sub-set test. */
			/* XXX This (and the sort of verts list) is better than systematic
			 *     search of all verts of one list into the other if lists have
			 *     a fair amount of elements.
			 *     Not sure however it's worth it in this case?
			 *     But as we also need sorted vert list to check verts multi-used
			 *     (in first pass of checks)... */
			/* XXX If we consider only "equal" polys (i.e. using exactly same set of verts)
			 *     as invalid, better to replace this by a simple memory cmp... */
			while ((p1_nv && p2_nv) && (p1_sub || p2_sub)) {
				if (*p1_v < *p2_v) {
					if (p1_sub)
						p1_sub = false;
					p1_nv--;
					p1_v++;
				}
				else if (*p2_v < *p1_v) {
					if (p2_sub)
						p2_sub = false;
					p2_nv--;
					p2_v++;
				}
				else {
					/* Equality, both next verts. */
					p1_nv--;
					p2_nv--;
					p1_v++;
					p2_v++;
				}
			}
			if (p1_nv && p1_sub)
				p1_sub = false;
			else if (p2_nv && p2_sub)
				p2_sub = false;

			if (p1_sub && p2_sub) {
				PRINT("\tPolys %u and %u use same vertices, considering poly %u as invalid.\n",
				      prev_sp->index, sp->index, sp->index);
				sp->invalid = true;
			}
			/* XXX In fact, these might be valid? :/ */
			else if (p1_sub) {
				PRINT("\t%u is a sub-poly of %u, considering it as invalid.\n", sp->index, prev_sp->index);
				sp->invalid = true;
			}
			else if (p2_sub) {
				PRINT("\t%u is a sub-poly of %u, considering it as invalid.\n", prev_sp->index, sp->index);
				prev_sp->invalid = true;
				prev_sp = sp; /* sp is new reference poly. */
			}
#else
			if (0) {
				p1_sub += 0;
				p2_sub += 0;
			}
			if ((p1_nv == p2_nv) && (memcmp(p1_v, p2_v, p1_nv * sizeof(*p1_v)) == 0)) {
				if (do_verbose) {
					PRINT_ERR("\tPolys %u and %u use same vertices (%u",
					          prev_sp->index, sp->index, *p1_v);
					for (j = 1; j < p1_nv; j++)
						PRINT_ERR(", %u", p1_v[j]);
					PRINT_ERR("), considering poly %u as invalid.\n", sp->index);
				}
				else {
					is_valid = false;
				}
				sp->invalid = true;
			}
#endif
			else {
				prev_sp = sp;
			}
		}

		/* Third check pass, testing loops used by none or more than one poly. */
		qsort(sort_polys, totpoly, sizeof(SortPoly), search_polyloop_cmp);
		sp = sort_polys;
		prev_sp = NULL;
		prev_end = 0;
		for (i = 0; i < totpoly; i++, sp++) {
			/* Free this now, we don't need it anymore, and avoid us another loop! */
			if (sp->verts)
				MEM_freeN(sp->verts);

			/* Note above prev_sp: in following code, we make sure it is always valid poly (or NULL). */
			if (sp->invalid) {
				if (do_fixes) {
					REMOVE_POLY_TAG((&mpolys[sp->index]));
					/* DO NOT REMOVE ITS LOOPS!!!
					 * As already invalid polys are at the end of the SortPoly list, the loops they
					 * were the only users have already been tagged as "to remove" during previous
					 * iterations, and we don't want to remove some loops that may be used by
					 * another valid poly! */
				}
			}
			/* Test loops users. */
			else {
				/* Unused loops. */
				if (prev_end < sp->loopstart) {
					for (j = prev_end, ml = &mloops[prev_end]; j < sp->loopstart; j++, ml++) {
						PRINT_ERR("\tLoop %u is unused.\n", j);
						if (do_fixes)
							REMOVE_LOOP_TAG(ml);
					}
					prev_end = sp->loopstart + sp->numverts;
					prev_sp = sp;
				}
				/* Multi-used loops. */
				else if (prev_end > sp->loopstart) {
					PRINT_ERR("\tPolys %u and %u share loops from %u to %u, considering poly %u as invalid.\n",
					          prev_sp->index, sp->index, sp->loopstart, prev_end, sp->index);
					if (do_fixes) {
						REMOVE_POLY_TAG((&mpolys[sp->index]));
						/* DO NOT REMOVE ITS LOOPS!!!
						 * They might be used by some next, valid poly!
						 * Just not updating prev_end/prev_sp vars is enough to ensure the loops
						 * effectively no more needed will be marked as "to be removed"! */
					}
				}
				else {
					prev_end = sp->loopstart + sp->numverts;
					prev_sp = sp;
				}
			}
		}
		/* We may have some remaining unused loops to get rid of! */
		if (prev_end < totloop) {
			for (j = prev_end, ml = &mloops[prev_end]; j < totloop; j++, ml++) {
				PRINT_ERR("\tLoop %u is unused.\n", j);
				if (do_fixes)
					REMOVE_LOOP_TAG(ml);
			}
		}

		MEM_freeN(sort_polys);
	}

	BLI_edgehash_free(edge_hash, NULL);

	/* fix deform verts */
	if (dverts) {
		MDeformVert *dv;
		for (i = 0, dv = dverts; i < totvert; i++, dv++) {
			MDeformWeight *dw;

			for (j = 0, dw = dv->dw; j < dv->totweight; j++, dw++) {
				/* note, greater then max defgroups is accounted for in our code, but not < 0 */
				if (!finite(dw->weight)) {
					PRINT_ERR("\tVertex deform %u, group %d has weight: %f\n", i, dw->def_nr, dw->weight);
					if (do_fixes) {
						dw->weight = 0.0f;
						vert_weights_fixed = true;
					}
				}
				else if (dw->weight < 0.0f || dw->weight > 1.0f) {
					PRINT_ERR("\tVertex deform %u, group %d has weight: %f\n", i, dw->def_nr, dw->weight);
					if (do_fixes) {
						CLAMP(dw->weight, 0.0f, 1.0f);
						vert_weights_fixed = true;
					}
				}

				if (dw->def_nr < 0) {
					PRINT_ERR("\tVertex deform %u, has invalid group %d\n", i, dw->def_nr);
					if (do_fixes) {
						defvert_remove_group(dv, dw);
						if (dv->dw) {
							/* re-allocated, the new values compensate for stepping
							 * within the for loop and may not be valid */
							j--;
							dw = dv->dw + j;

							vert_weights_fixed = true;
						}
						else { /* all freed */
							break;
						}
					}
				}
			}
		}
	}

#   undef REMOVE_EDGE_TAG
#   undef IS_REMOVED_EDGE
#   undef REMOVE_LOOP_TAG
#   undef REMOVE_POLY_TAG

	if (mesh) {
		if (do_face_free) {
			BKE_mesh_strip_loose_faces(mesh);
		}

		if (do_polyloop_free) {
			BKE_mesh_strip_loose_polysloops(mesh);
		}

		if (do_edge_free) {
			BKE_mesh_strip_loose_edges(mesh);
		}

		if (do_edge_recalc) {
			BKE_mesh_calc_edges(mesh, true, false);
		}
	}

	if (mesh && mesh->mselect) {
		MSelect *msel;
		bool free_msel = false;

		for (i = 0, msel = mesh->mselect; i < mesh->totselect; i++, msel++) {
			int tot_elem = 0;

			if (msel->index < 0) {
				PRINT_ERR("\tMesh select element %d type %d index is negative, "
				          "resetting selection stack.\n", i, msel->type);
				free_msel = true;
				break;
			}

			switch (msel->type) {
				case ME_VSEL:
					tot_elem = mesh->totvert;
					break;
				case ME_ESEL:
					tot_elem = mesh->totedge;
					break;
				case ME_FSEL:
					tot_elem = mesh->totface;
					break;
			}

			if (msel->index > tot_elem) {
				PRINT_ERR("\tMesh select element %d type %d index %d is larger than data array size %d, "
				          "resetting selection stack.\n", i, msel->type, msel->index, tot_elem);

				free_msel = true;
				break;
			}
		}

		if (free_msel) {
			MEM_freeN(mesh->mselect);
			mesh->mselect = NULL;
			mesh->totselect = 0;
		}
	}

	PRINT_MSG("%s: finished\n\n", __func__);

	*r_changed = (verts_fixed || vert_weights_fixed || do_polyloop_free || do_edge_free || do_edge_recalc || msel_fixed);

	return is_valid;
}

static bool mesh_validate_customdata(CustomData *data, CustomDataMask mask,
                                     const bool do_verbose, const bool do_fixes,
                                     bool *r_change)
{
	bool is_valid = true;
	bool has_fixes = false;
	int i = 0;

	PRINT_MSG("%s: Checking %d CD layers...\n", __func__, data->totlayer);

	while (i < data->totlayer) {
		CustomDataLayer *layer = &data->layers[i];
		bool ok = true;

		if (CustomData_layertype_is_singleton(layer->type)) {
			const int layer_tot = CustomData_number_of_layers(data, layer->type);
			if (layer_tot > 1) {
				PRINT_ERR("\tCustomDataLayer type %d is a singleton, found %d in Mesh structure\n",
				          layer->type, layer_tot);
				ok = false;
			}
		}

		if (mask != 0) {
			CustomDataMask layer_typemask = CD_TYPE_AS_MASK(layer->type);
			if ((layer_typemask & mask) == 0) {
				PRINT_ERR("\tCustomDataLayer type %d which isn't in the mask\n",
				          layer->type);
				ok = false;
			}
		}

		if (ok == false) {
			if (do_fixes) {
				CustomData_free_layer(data, layer->type, 0, i);
				has_fixes = true;
			}
		}

		if (ok)
			i++;
	}

	PRINT_MSG("%s: Finished (is_valid=%d)\n\n", __func__, (int)!has_fixes);

	*r_change = has_fixes;

	return is_valid;
}

/**
 * \returns is_valid.
 */
bool BKE_mesh_validate_all_customdata(CustomData *vdata, CustomData *edata,
                                      CustomData *ldata, CustomData *pdata,
                                      const bool check_meshmask,
                                      const bool do_verbose, const bool do_fixes,
                                      bool *r_change)
{
	bool is_valid = true;
	bool is_change_v, is_change_e, is_change_l, is_change_p;
	int tot_texpoly, tot_uvloop, tot_vcolloop;
	CustomDataMask mask = check_meshmask ? CD_MASK_MESH : 0;

	is_valid &= mesh_validate_customdata(vdata, mask, do_verbose, do_fixes, &is_change_v);
	is_valid &= mesh_validate_customdata(edata, mask, do_verbose, do_fixes, &is_change_e);
	is_valid &= mesh_validate_customdata(ldata, mask, do_verbose, do_fixes, &is_change_l);
	is_valid &= mesh_validate_customdata(pdata, mask, do_verbose, do_fixes, &is_change_p);

	tot_texpoly = CustomData_number_of_layers(pdata, CD_MTEXPOLY);
	tot_uvloop = CustomData_number_of_layers(ldata, CD_MLOOPUV);
	tot_vcolloop = CustomData_number_of_layers(ldata, CD_MLOOPCOL);
	if (tot_texpoly != tot_uvloop) {
		PRINT_ERR("\tCustomDataLayer mismatch, tot_texpoly(%d), tot_uvloop(%d)\n",
		          tot_texpoly, tot_uvloop);
	}
	if (tot_texpoly > MAX_MTFACE) {
		PRINT_ERR("\tMore UV layers than %d allowed, %d last ones won't be available for render, shaders, etc.\n",
		          MAX_MTFACE, tot_texpoly - MAX_MTFACE);
	}
	if (tot_uvloop > MAX_MTFACE) {
		PRINT_ERR("\tMore UV layers than %d allowed, %d last ones won't be available for render, shaders, etc.\n",
		          MAX_MTFACE, tot_uvloop - MAX_MTFACE);
	}
	if (tot_vcolloop > MAX_MCOL) {
		PRINT_ERR("\tMore VCol layers than %d allowed, %d last ones won't be available for render, shaders, etc.\n",
		          MAX_MCOL, tot_vcolloop - MAX_MCOL);
	}

	*r_change = (is_change_v || is_change_e || is_change_l || is_change_p);

	return is_valid;
}

/**
 * \see  #DM_is_valid to call on derived meshes
 *
 * \returns true if a change is made.
 */
int BKE_mesh_validate(Mesh *me, const int do_verbose)
{
	bool is_valid = true;
	bool changed;

	if (do_verbose) {
		printf("MESH: %s\n", me->id.name + 2);
	}

	is_valid &= BKE_mesh_validate_all_customdata(
	        &me->vdata, &me->edata, &me->ldata, &me->pdata,
	        true,
	        do_verbose, true,
	        &changed);

	is_valid &= BKE_mesh_validate_arrays(
	        me,
	        me->mvert, me->totvert,
	        me->medge, me->totedge,
	        me->mface, me->totface,
	        me->mloop, me->totloop,
	        me->mpoly, me->totpoly,
	        me->dvert,
	        do_verbose, true,
	        &changed);

	if (changed) {
		DAG_id_tag_update(&me->id, OB_RECALC_DATA);
		return true;
	}
	else {
		return false;
	}
}

/**
 * Duplicate of BM_mesh_cd_validate() for Mesh data.
 */
void BKE_mesh_cd_validate(Mesh *me)
{
	int totlayer_mtex = CustomData_number_of_layers(&me->pdata, CD_MTEXPOLY);
	int totlayer_uv = CustomData_number_of_layers(&me->ldata, CD_MLOOPUV);
	int totlayer_mcol = CustomData_number_of_layers(&me->ldata, CD_MLOOPCOL);
	int mtex_index = CustomData_get_layer_index(&me->pdata, CD_MTEXPOLY);
	int uv_index = CustomData_get_layer_index(&me->ldata, CD_MLOOPUV);
	int i;

	/* XXX For now, do not delete those, just warn they are not really usable. */
	if (UNLIKELY(totlayer_mtex > MAX_MTFACE)) {
		printf("WARNING! More UV layers than %d allowed, %d last ones won't be available for render, shaders, etc.\n",
		       MAX_MTFACE, totlayer_mtex - MAX_MTFACE);
	}
	if (UNLIKELY(totlayer_uv > MAX_MTFACE)) {
		printf("WARNING! More UV layers than %d allowed, %d last ones won't be available for render, shaders, etc.\n",
		       MAX_MTFACE, totlayer_uv - MAX_MTFACE);
	}
	if (UNLIKELY(totlayer_mcol > MAX_MCOL)) {
		printf("WARNING! More VCol layers than %d allowed, %d last ones won't be available for render, shaders, etc.\n",
		       MAX_MCOL, totlayer_mcol - MAX_MCOL);
	}

	if (LIKELY(totlayer_mtex == totlayer_uv)) {
		/* pass */
	}
	else if (totlayer_mtex < totlayer_uv) {
		do {
			const char *from_name =  me->ldata.layers[uv_index + totlayer_mtex].name;
			CustomData_add_layer_named(&me->pdata, CD_MTEXPOLY, CD_DEFAULT, NULL, me->totpoly, from_name);
			CustomData_set_layer_unique_name(&me->pdata, totlayer_mtex);
		} while (totlayer_uv != ++totlayer_mtex);
		mtex_index = CustomData_get_layer_index(&me->pdata, CD_MTEXPOLY);
	}
	else if (totlayer_uv < totlayer_mtex) {
		do {
			const char *from_name = me->pdata.layers[mtex_index + totlayer_uv].name;
			CustomData_add_layer_named(&me->ldata, CD_MLOOPUV, CD_DEFAULT, NULL, me->totloop, from_name);
			CustomData_set_layer_unique_name(&me->ldata, totlayer_uv);
		} while (totlayer_mtex != ++totlayer_uv);
		uv_index = CustomData_get_layer_index(&me->ldata, CD_MLOOPUV);
	}

	BLI_assert(totlayer_mtex == totlayer_uv);

	/* Check uv/tex names match as well!!! */
	for (i = 0; i < totlayer_mtex; i++, mtex_index++, uv_index++) {
		const char *name_src = me->pdata.layers[mtex_index].name;
		const char *name_dst = me->ldata.layers[uv_index].name;
		if (!STREQ(name_src, name_dst)) {
			BKE_mesh_uv_cdlayer_rename_index(me, mtex_index, uv_index, -1, name_src, false);
		}
	}
}

/**
 * Check all material indices of polygons are valid, invalid ones are set to 0.
 * \returns is_valid.
 */
int BKE_mesh_validate_material_indices(Mesh *me)
{
	MPoly *mp;
	const int max_idx = max_ii(0, me->totcol - 1);
	const int totpoly = me->totpoly;
	int i;
	bool is_valid = true;

	for (mp = me->mpoly, i = 0; i < totpoly; i++, mp++) {
		if (mp->mat_nr > max_idx) {
			mp->mat_nr = 0;
			is_valid = false;
		}
	}

	if (!is_valid) {
		DAG_id_tag_update(&me->id, OB_RECALC_DATA);
		return true;
	}
	else {
		return false;
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Mesh Stripping (removing invalid data)
 * \{ */

/* We need to keep this for edge creation (for now?), and some old readfile code... */
void BKE_mesh_strip_loose_faces(Mesh *me)
{
	MFace *f;
	int a, b;

	for (a = b = 0, f = me->mface; a < me->totface; a++, f++) {
		if (f->v3) {
			if (a != b) {
				memcpy(&me->mface[b], f, sizeof(me->mface[b]));
				CustomData_copy_data(&me->fdata, &me->fdata, a, b, 1);
			}
			b++;
		}
	}
	if (a != b) {
		CustomData_free_elem(&me->fdata, b, a - b);
		me->totface = b;
	}
}

/* Works on both loops and polys! */
/* Note: It won't try to guess which loops of an invalid poly to remove!
 *       this is the work of the caller, to mark those loops...
 *       See e.g. BKE_mesh_validate_arrays(). */
void BKE_mesh_strip_loose_polysloops(Mesh *me)
{
	MPoly *p;
	MLoop *l;
	int a, b;
	/* New loops idx! */
	int *new_idx = MEM_mallocN(sizeof(int) * me->totloop, __func__);

	for (a = b = 0, p = me->mpoly; a < me->totpoly; a++, p++) {
		bool invalid = false;
		int i = p->loopstart;
		int stop = i + p->totloop;

		if (stop > me->totloop || stop < i) {
			invalid = true;
		}
		else {
			l = &me->mloop[i];
			i = stop - i;
			/* If one of the poly's loops is invalid, the whole poly is invalid! */
			for (; i--; l++) {
				if (l->e == INVALID_LOOP_EDGE_MARKER) {
					invalid = true;
					break;
				}
			}
		}

		if (p->totloop >= 3 && !invalid) {
			if (a != b) {
				memcpy(&me->mpoly[b], p, sizeof(me->mpoly[b]));
				CustomData_copy_data(&me->pdata, &me->pdata, a, b, 1);
			}
			b++;
		}
	}
	if (a != b) {
		CustomData_free_elem(&me->pdata, b, a - b);
		me->totpoly = b;
	}

	/* And now, get rid of invalid loops. */
	for (a = b = 0, l = me->mloop; a < me->totloop; a++, l++) {
		if (l->e != INVALID_LOOP_EDGE_MARKER) {
			if (a != b) {
				memcpy(&me->mloop[b], l, sizeof(me->mloop[b]));
				CustomData_copy_data(&me->ldata, &me->ldata, a, b, 1);
			}
			new_idx[a] = b;
			b++;
		}
		else {
			/* XXX Theoretically, we should be able to not do this, as no remaining poly
			 *     should use any stripped loop. But for security's sake... */
			new_idx[a] = -a;
		}
	}
	if (a != b) {
		CustomData_free_elem(&me->ldata, b, a - b);
		me->totloop = b;
	}

	/* And now, update polys' start loop index. */
	/* Note: At this point, there should never be any poly using a striped loop! */
	for (a = 0, p = me->mpoly; a < me->totpoly; a++, p++) {
		p->loopstart = new_idx[p->loopstart];
	}

	MEM_freeN(new_idx);
}

void BKE_mesh_strip_loose_edges(Mesh *me)
{
	MEdge *e;
	MLoop *l;
	int a, b;
	unsigned int *new_idx = MEM_mallocN(sizeof(int) * me->totedge, __func__);

	for (a = b = 0, e = me->medge; a < me->totedge; a++, e++) {
		if (e->v1 != e->v2) {
			if (a != b) {
				memcpy(&me->medge[b], e, sizeof(me->medge[b]));
				CustomData_copy_data(&me->edata, &me->edata, a, b, 1);
			}
			new_idx[a] = b;
			b++;
		}
		else {
			new_idx[a] = INVALID_LOOP_EDGE_MARKER;
		}
	}
	if (a != b) {
		CustomData_free_elem(&me->edata, b, a - b);
		me->totedge = b;
	}

	/* And now, update loops' edge indices. */
	/* XXX We hope no loop was pointing to a striped edge!
	 *     Else, its e will be set to INVALID_LOOP_EDGE_MARKER :/ */
	for (a = 0, l = me->mloop; a < me->totloop; a++, l++) {
		l->e = new_idx[l->e];
	}

	MEM_freeN(new_idx);
}
/** \} */



/* -------------------------------------------------------------------- */

/** \name Mesh Edge Calculation
 * \{ */

/* make edges in a Mesh, for outside of editmode */

struct EdgeSort {
	unsigned int v1, v2;
	char is_loose, is_draw;
};

/* edges have to be added with lowest index first for sorting */
static void to_edgesort(struct EdgeSort *ed,
                        unsigned int v1, unsigned int v2,
                        char is_loose, short is_draw)
{
	if (v1 < v2) {
		ed->v1 = v1; ed->v2 = v2;
	}
	else {
		ed->v1 = v2; ed->v2 = v1;
	}
	ed->is_loose = is_loose;
	ed->is_draw = is_draw;
}

static int vergedgesort(const void *v1, const void *v2)
{
	const struct EdgeSort *x1 = v1, *x2 = v2;

	if (x1->v1 > x2->v1) return 1;
	else if (x1->v1 < x2->v1) return -1;
	else if (x1->v2 > x2->v2) return 1;
	else if (x1->v2 < x2->v2) return -1;

	return 0;
}


/* Create edges based on known verts and faces,
 * this function is only used when loading very old blend files */

static void mesh_calc_edges_mdata(
        MVert *UNUSED(allvert), MFace *allface, MLoop *allloop,
        MPoly *allpoly, int UNUSED(totvert), int totface, int UNUSED(totloop), int totpoly,
        const bool use_old,
        MEdge **r_medge, int *r_totedge)
{
	MPoly *mpoly;
	MFace *mface;
	MEdge *medge, *med;
	EdgeHash *hash;
	struct EdgeSort *edsort, *ed;
	int a, totedge = 0;
	unsigned int totedge_final = 0;
	unsigned int edge_index;

	/* we put all edges in array, sort them, and detect doubles that way */

	for (a = totface, mface = allface; a > 0; a--, mface++) {
		if (mface->v4) totedge += 4;
		else if (mface->v3) totedge += 3;
		else totedge += 1;
	}

	if (totedge == 0) {
		/* flag that mesh has edges */
		(*r_medge) = MEM_callocN(0, __func__);
		(*r_totedge) = 0;
		return;
	}

	ed = edsort = MEM_mallocN(totedge * sizeof(struct EdgeSort), "EdgeSort");

	for (a = totface, mface = allface; a > 0; a--, mface++) {
		to_edgesort(ed++, mface->v1, mface->v2, !mface->v3, mface->edcode & ME_V1V2);
		if (mface->v4) {
			to_edgesort(ed++, mface->v2, mface->v3, 0, mface->edcode & ME_V2V3);
			to_edgesort(ed++, mface->v3, mface->v4, 0, mface->edcode & ME_V3V4);
			to_edgesort(ed++, mface->v4, mface->v1, 0, mface->edcode & ME_V4V1);
		}
		else if (mface->v3) {
			to_edgesort(ed++, mface->v2, mface->v3, 0, mface->edcode & ME_V2V3);
			to_edgesort(ed++, mface->v3, mface->v1, 0, mface->edcode & ME_V3V1);
		}
	}

	qsort(edsort, totedge, sizeof(struct EdgeSort), vergedgesort);

	/* count final amount */
	for (a = totedge, ed = edsort; a > 1; a--, ed++) {
		/* edge is unique when it differs from next edge, or is last */
		if (ed->v1 != (ed + 1)->v1 || ed->v2 != (ed + 1)->v2) totedge_final++;
	}
	totedge_final++;

	medge = MEM_callocN(sizeof(MEdge) * totedge_final, __func__);

	for (a = totedge, med = medge, ed = edsort; a > 1; a--, ed++) {
		/* edge is unique when it differs from next edge, or is last */
		if (ed->v1 != (ed + 1)->v1 || ed->v2 != (ed + 1)->v2) {
			med->v1 = ed->v1;
			med->v2 = ed->v2;
			if (use_old == false || ed->is_draw) med->flag = ME_EDGEDRAW | ME_EDGERENDER;
			if (ed->is_loose) med->flag |= ME_LOOSEEDGE;

			/* order is swapped so extruding this edge as a surface wont flip face normals
			 * with cyclic curves */
			if (ed->v1 + 1 != ed->v2) {
				SWAP(unsigned int, med->v1, med->v2);
			}
			med++;
		}
		else {
			/* equal edge, we merge the drawflag */
			(ed + 1)->is_draw |= ed->is_draw;
		}
	}
	/* last edge */
	med->v1 = ed->v1;
	med->v2 = ed->v2;
	med->flag = ME_EDGEDRAW;
	if (ed->is_loose) med->flag |= ME_LOOSEEDGE;
	med->flag |= ME_EDGERENDER;

	MEM_freeN(edsort);

	/* set edge members of mloops */
	hash = BLI_edgehash_new_ex(__func__, totedge_final);
	for (edge_index = 0, med = medge; edge_index < totedge_final; edge_index++, med++) {
		BLI_edgehash_insert(hash, med->v1, med->v2, SET_UINT_IN_POINTER(edge_index));
	}

	mpoly = allpoly;
	for (a = 0; a < totpoly; a++, mpoly++) {
		MLoop *ml, *ml_next;
		int i = mpoly->totloop;

		ml_next = allloop + mpoly->loopstart;  /* first loop */
		ml = &ml_next[i - 1];                  /* last loop */

		while (i-- != 0) {
			ml->e = GET_UINT_FROM_POINTER(BLI_edgehash_lookup(hash, ml->v, ml_next->v));
			ml = ml_next;
			ml_next++;
		}
	}

	BLI_edgehash_free(hash, NULL);

	*r_medge = medge;
	*r_totedge = totedge_final;
}

/**
 * If the mesh is from a very old blender version,
 * convert mface->edcode to edge drawflags
 */
void BKE_mesh_calc_edges_legacy(Mesh *me, const bool use_old)
{
	MEdge *medge;
	int totedge = 0;

	mesh_calc_edges_mdata(me->mvert, me->mface, me->mloop, me->mpoly,
	                      me->totvert, me->totface, me->totloop, me->totpoly,
	                      use_old, &medge, &totedge);

	if (totedge == 0) {
		/* flag that mesh has edges */
		me->medge = medge;
		me->totedge = 0;
		return;
	}

	medge = CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, totedge);
	me->medge = medge;
	me->totedge = totedge;

	BKE_mesh_strip_loose_faces(me);
}


/**
 * Calculate edges from polygons
 *
 * \param mesh  The mesh to add edges into
 * \param update  When true create new edges co-exist
 */
void BKE_mesh_calc_edges(Mesh *mesh, bool update, const bool select)
{
	CustomData edata;
	EdgeHashIterator *ehi;
	MPoly *mp;
	MEdge *med, *med_orig;
	EdgeHash *eh;
	unsigned int eh_reserve;
	int i, totedge, totpoly = mesh->totpoly;
	int med_index;
	/* select for newly created meshes which are selected [#25595] */
	const short ed_flag = (ME_EDGEDRAW | ME_EDGERENDER) | (select ? SELECT : 0);

	if (mesh->totedge == 0)
		update = false;

	eh_reserve = max_ii(update ? mesh->totedge : 0, BLI_EDGEHASH_SIZE_GUESS_FROM_POLYS(totpoly));
	eh = BLI_edgehash_new_ex(__func__, eh_reserve);

	if (update) {
		/* assume existing edges are valid
		 * useful when adding more faces and generating edges from them */
		med = mesh->medge;
		for (i = 0; i < mesh->totedge; i++, med++)
			BLI_edgehash_insert(eh, med->v1, med->v2, med);
	}

	/* mesh loops (bmesh only) */
	for (mp = mesh->mpoly, i = 0; i < totpoly; mp++, i++) {
		MLoop *l = &mesh->mloop[mp->loopstart];
		int j, l_prev = (l + (mp->totloop - 1))->v;
		for (j = 0; j < mp->totloop; j++, l++) {
			if (!BLI_edgehash_haskey(eh, l_prev, l->v)) {
				BLI_edgehash_insert(eh, l_prev, l->v, NULL);
			}
			l_prev = l->v;
		}
	}

	totedge = BLI_edgehash_size(eh);

	/* write new edges into a temporary CustomData */
	CustomData_reset(&edata);
	CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);

	med = CustomData_get_layer(&edata, CD_MEDGE);
	for (ehi = BLI_edgehashIterator_new(eh), i = 0;
	     BLI_edgehashIterator_isDone(ehi) == false;
	     BLI_edgehashIterator_step(ehi), ++i, ++med)
	{
		if (update && (med_orig = BLI_edgehashIterator_getValue(ehi))) {
			*med = *med_orig; /* copy from the original */
		}
		else {
			BLI_edgehashIterator_getKey(ehi, &med->v1, &med->v2);
			med->flag = ed_flag;
		}

		/* store the new edge index in the hash value */
		BLI_edgehashIterator_setValue(ehi, SET_INT_IN_POINTER(i));
	}
	BLI_edgehashIterator_free(ehi);

	if (mesh->totpoly) {
		/* second pass, iterate through all loops again and assign
		 * the newly created edges to them. */
		for (mp = mesh->mpoly, i = 0; i < mesh->totpoly; mp++, i++) {
			MLoop *l = &mesh->mloop[mp->loopstart];
			MLoop *l_prev = (l + (mp->totloop - 1));
			int j;
			for (j = 0; j < mp->totloop; j++, l++) {
				/* lookup hashed edge index */
				med_index = GET_INT_FROM_POINTER(BLI_edgehash_lookup(eh, l_prev->v, l->v));
				l_prev->e = med_index;
				l_prev = l;
			}
		}
	}

	/* free old CustomData and assign new one */
	CustomData_free(&mesh->edata, mesh->totedge);
	mesh->edata = edata;
	mesh->totedge = totedge;

	mesh->medge = CustomData_get_layer(&mesh->edata, CD_MEDGE);

	BLI_edgehash_free(eh, NULL);
}
/** \} */
