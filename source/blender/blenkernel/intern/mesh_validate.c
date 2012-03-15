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

#include "BLO_sys_types.h"

#include "BLI_edgehash.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"

#include "MEM_guardedalloc.h"

#define SELECT 1

/* Used to detect polys (faces) using exactly the same vertices. */
/* Used to detect loops used by no (disjoint) or more than one (intersect) polys. */
typedef struct SortPoly {
	int *verts;
	int numverts;
	int loopstart;
	unsigned int index;
	int invalid; /* Poly index. */
} SortPoly;

/* TODO check there is not some standard define of this somewhere! */
static int int_cmp(const void *v1, const void *v2)
{
	return *(int*)v1 > *(int*)v2 ? 1 : *(int*)v1 < *(int*)v2 ? -1 : 0;
}

static int search_poly_cmp(const void *v1, const void *v2)
{
	const SortPoly *sp1 = v1, *sp2 = v2;
	const int max_idx = sp1->numverts > sp2->numverts ? sp2->numverts : sp1->numverts;
	int idx = 0;

	/* Reject all invalid polys at end of list! */
	if (sp1->invalid || sp2->invalid)
		return sp1->invalid && sp2->invalid ? 0 : sp1->invalid ? 1 : -1;
	/* Else, sort on first non-egal verts (remember verts of valid polys are sorted). */
	while (idx < max_idx && sp1->verts[idx] == sp2->verts[idx])
		idx++;
	return sp1->verts[idx] > sp2->verts[idx] ? 1 : sp1->verts[idx] < sp2->verts[idx] ? -1 :
	       sp1->numverts > sp2->numverts ? 1 : sp1->numverts < sp2->numverts ? -1 : 0;
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

#define PRINT if(do_verbose) printf

int BKE_mesh_validate_arrays(Mesh *mesh,
                             MVert *mverts, unsigned int totvert,
                             MEdge *medges, unsigned int totedge,
                             MLoop *mloops, unsigned int totloop,
                             MPoly *mpolys, unsigned int totpoly,
                             MDeformVert *dverts, /* assume totvert length */
                             const short do_verbose, const short do_fixes)
{
#	define REMOVE_EDGE_TAG(_me) { _me->v2 = _me->v1; do_edge_free = TRUE; }
#	define IS_REMOVED_EDGE(_me) (_me->v2 == _me->v1)

#	define REMOVE_LOOP_TAG(_ml) { _ml->e = INVALID_LOOP_EDGE_MARKER; do_polyloop_free = TRUE; }
#	define REMOVE_POLY_TAG(_mp) { _mp->totloop *= -1; do_polyloop_free = TRUE; }

	MVert *mv = mverts;
	MEdge *me;
	MLoop *ml;
	MPoly *mp;
	unsigned int i, j;
	int *v;

	short do_edge_free= FALSE;
	short do_polyloop_free= FALSE; /* This regroups loops and polys! */

	short verts_fixed= FALSE;
	short vert_weights_fixed= FALSE;

	int do_edge_recalc= FALSE;

	EdgeHash *edge_hash = BLI_edgehash_new();

	BLI_assert(!(do_fixes && mesh == NULL));

	PRINT("%s: verts(%u), edges(%u), loops(%u), polygons(%u)\n",
	      __func__, totvert, totedge, totloop, totpoly);

	if(totedge == 0 && totpoly != 0) {
		PRINT("    logical error, %u polygons and 0 edges\n", totpoly);
		do_edge_recalc = do_fixes;
	}

	for(i=1; i<totvert; i++, mv++) {
		int j;
		int fix_normal= TRUE;

		for(j=0; j<3; j++) {
			if(!finite(mv->co[j])) {
				PRINT("    vertex %u: has invalid coordinate\n", i);

				if (do_fixes) {
					zero_v3(mv->co);

					verts_fixed= TRUE;
				}
			}

			if(mv->no[j]!=0)
				fix_normal= FALSE;
		}

		if(fix_normal) {
			PRINT("    vertex %u: has zero normal, assuming Z-up normal\n", i);
			if (do_fixes) {
				mv->no[2]= SHRT_MAX;
				verts_fixed= TRUE;
			}
		}
	}

	for(i=0, me= medges; i<totedge; i++, me++) {
		int remove= FALSE;
		if(me->v1 == me->v2) {
			PRINT("    edge %u: has matching verts, both %u\n", i, me->v1);
			remove= do_fixes;
		}
		if(me->v1 >= totvert) {
			PRINT("    edge %u: v1 index out of range, %u\n", i, me->v1);
			remove= do_fixes;
		}
		if(me->v2 >= totvert) {
			PRINT("    edge %u: v2 index out of range, %u\n", i, me->v2);
			remove= do_fixes;
		}

		if(BLI_edgehash_haskey(edge_hash, me->v1, me->v2)) {
			PRINT("    edge %u: is a duplicate of %d\n", i,
			      GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, me->v1, me->v2)));
			remove= do_fixes;
		}

		if(remove == FALSE) {
			BLI_edgehash_insert(edge_hash, me->v1, me->v2, SET_INT_IN_POINTER(i));
		}
		else {
			REMOVE_EDGE_TAG(me);
		}
	}

	/* Checking loops and polys is a bit tricky, as they are quite intricated...
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
	 * so be sure to leave at most one poly/loop!
	 */
	{
		SortPoly *sort_polys = MEM_callocN(sizeof(SortPoly) * totpoly, "mesh validate's sort_polys");
		SortPoly *prev_sp, *sp = sort_polys;
		int prev_end;
		for (i = 0, mp = mpolys; i < totpoly; i++, mp++, sp++) {
			sp->index = i;

			if (mp->loopstart < 0 || mp->totloop < 3) {
				/* Invalid loop data. */
				PRINT("    poly %u is invalid (loopstart: %u, totloop: %u)\n", sp->index, mp->loopstart, mp->totloop);
				sp->invalid = TRUE;
			}
			else if (mp->loopstart + mp->totloop > totloop) {
				/* Invalid loop data. */
				PRINT("    poly %u uses loops out of range (loopstart: %u, loopend: %u, max nbr of loops: %u)\n",
				      sp->index, mp->loopstart, mp->loopstart + mp->totloop - 1, totloop - 1);
				sp->invalid = TRUE;
			}
			else {
				/* Poly itself is valid, for now. */
				int v1, v2; /* v1 is prev loop vert idx, v2 is current loop one. */
				sp->invalid = FALSE;
				sp->verts = v = MEM_mallocN(sizeof(int) * mp->totloop, "Vert idx of SortPoly");
				sp->numverts = mp->totloop;
				sp->loopstart = mp->loopstart;

				/* Test all poly's loops' vert idx. */
				for (j = 0, ml = &mloops[sp->loopstart]; j < mp->totloop; j++, ml++, v++) {
					if (ml->v >= totvert) {
						/* Invalid vert idx. */
						PRINT("    loop %u has invalid vert reference (%u)\n", sp->loopstart + j, ml->v);
						sp->invalid = TRUE;
					}
					*v = ml->v;
				}

				if (sp->invalid)
					continue;

				/* Test all poly's loops. */
				for (j = 0, ml = &mloops[sp->loopstart]; j < mp->totloop; j++, ml++) {
					v1 = ml->v;
					v2 = mloops[sp->loopstart + (j + 1) % mp->totloop].v;
					if (!BLI_edgehash_haskey(edge_hash, v1, v2)) {
						/* Edge not existing. */
						PRINT("    poly %u needs missing edge (%u, %u)\n", sp->index, v1, v2);
						if (do_fixes)
							do_edge_recalc = TRUE;
						else
							sp->invalid = TRUE;
					}
					else if (ml->e >= totedge) {
						/* Invalid edge idx.
						 * We already know from previous text that a valid edge exists, use it (if allowed)! */
						if(do_fixes) {
							int prev_e = ml->e;
							ml->e = GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, v1, v2));
							PRINT("    loop %u has invalid edge reference (%u), fixed using edge %u\n",
							      sp->loopstart + j, prev_e, ml->e);
						}
						else {
							PRINT("    loop %u has invalid edge reference (%u)\n", sp->loopstart + j, ml->e);
							sp->invalid = TRUE;
						}
					}
					else {
						me = &medges[ml->e];
						if (IS_REMOVED_EDGE(me) || !((me->v1 == v1 && me->v2 == v2) || (me->v1 == v2 && me->v2 == v1))) {
							/* The pointed edge is invalid (tagged as removed, or vert idx mismatch),
							 * and we already know from previous test that a valid one exists, use it (if allowed)! */
							if(do_fixes) {
								int prev_e = ml->e;
								ml->e = GET_INT_FROM_POINTER(BLI_edgehash_lookup(edge_hash, v1, v2));
								PRINT("    poly %u has invalid edge reference (%u), fixed using edge %u\n",
									  sp->index, prev_e, ml->e);
							}
							else {
								PRINT("    poly %u has invalid edge reference (%u)\n", sp->index, ml->e);
								sp->invalid = TRUE;
							}
						}
					}
				}

				/* Now check that that poly does not use a same vertex more than once! */
				if (!sp->invalid) {
					int *prev_v = v = sp->verts;
					j = sp->numverts;

					qsort(sp->verts, j, sizeof(int), int_cmp);

					for (j--, v++; j; j--, v++) {
						if (*v != *prev_v) {
							int dlt = v - prev_v;
							if (dlt > 1) {
								PRINT("    poly %u is invalid, it multi-uses vertex %u (%u times)\n",
								      sp->index, *prev_v, dlt);
								sp->invalid = TRUE;
							}
							prev_v = v;
						}
					}
					if (v - prev_v > 1) { /* Don’t forget final verts! */
						PRINT("    poly %u is invalid, it multi-uses vertex %u (%u times)\n",
						      sp->index, *prev_v, (int)(v - prev_v));
						sp->invalid = TRUE;
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
			int *p1_v = sp->verts, *p2_v = prev_sp->verts;
			short p1_sub = TRUE, p2_sub = TRUE;
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
						p1_sub = FALSE;
					p1_nv--;
					p1_v++;
				}
				else if (*p2_v < *p1_v) {
					if (p2_sub)
						p2_sub = FALSE;
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
				p1_sub = FALSE;
			else if (p2_nv && p2_sub)
				p2_sub = FALSE;

			if (p1_sub && p2_sub) {
				PRINT("    polys %u and %u use same vertices, considering poly %u as invalid.\n",
				      prev_sp->index, sp->index, sp->index);
				sp->invalid = TRUE;
			}
			/* XXX In fact, these might be valid? :/ */
			else if (p1_sub) {
				PRINT("    %u is a sub-poly of %u, considering it as invalid.\n", sp->index, prev_sp->index);
				sp->invalid = TRUE;
			}
			else if (p2_sub) {
				PRINT("    %u is a sub-poly of %u, considering it as invalid.\n", prev_sp->index, sp->index);
				prev_sp->invalid = TRUE;
				prev_sp = sp; /* sp is new reference poly. */
			}
#else
			if (0) {
				p1_sub += 0;
				p2_sub += 0;
			}
			if((p1_nv == p2_nv) && (memcmp(p1_v, p2_v, p1_nv * sizeof(*p1_v)) == 0)) {
				if (do_verbose) {
					PRINT("    polys %u and %u use same vertices (%u",
					      prev_sp->index, sp->index, *p1_v);
					for (j = 1; j < p1_nv; j++)
						PRINT(", %u", p1_v[j]);
					PRINT("), considering poly %u as invalid.\n", sp->index);
				}
				sp->invalid = TRUE;
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
					 * iterations, and we don’t want to remove some loops that may be used by
					 * another valid poly! */
				}
			}
			/* Test loops users. */
			else {
				/* Unused loops. */
				if (prev_end < sp->loopstart) {
					for (j = prev_end, ml = &mloops[prev_end]; j < sp->loopstart; j++, ml++) {
						PRINT("    loop %u is unused.\n", j);
						if (do_fixes)
							REMOVE_LOOP_TAG(ml);
					}
					prev_end = sp->loopstart + sp->numverts;
					prev_sp = sp;
				}
				/* Multi-used loops. */
				else if (prev_end > sp->loopstart) {
					PRINT("    polys %u and %u share loops from %u to %u, considering poly %u as invalid.\n",
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
				PRINT("    loop %u is unused.\n", j);
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
		for(i=0, dv= dverts; i<totvert; i++, dv++) {
			MDeformWeight *dw;
			unsigned int j;

			for(j=0, dw= dv->dw; j < dv->totweight; j++, dw++) {
				/* note, greater then max defgroups is accounted for in our code, but not < 0 */
				if (!finite(dw->weight)) {
					PRINT("    vertex deform %u, group %d has weight: %f\n", i, dw->def_nr, dw->weight);
					if (do_fixes) {
						dw->weight= 0.0f;
						vert_weights_fixed= TRUE;
					}
				}
				else if (dw->weight < 0.0f || dw->weight > 1.0f) {
					PRINT("    vertex deform %u, group %d has weight: %f\n", i, dw->def_nr, dw->weight);
					if (do_fixes) {
						CLAMP(dw->weight, 0.0f, 1.0f);
						vert_weights_fixed= TRUE;
					}
				}

				if (dw->def_nr < 0) {
					PRINT("    vertex deform %u, has invalid group %d\n", i, dw->def_nr);
					if (do_fixes) {
						defvert_remove_group(dv, dw);
						if (dv->dw) {
							/* re-allocated, the new values compensate for stepping
							 * within the for loop and may not be valid */
							j--;
							dw= dv->dw + j;

							vert_weights_fixed= TRUE;
						}
						else { /* all freed */
							break;
						}
					}
				}
			}
		}
	}

	PRINT("BKE_mesh_validate: finished\n\n");

#	undef REMOVE_EDGE_TAG
#	undef IS_REMOVED_EDGE
#	undef REMOVE_LOOP_TAG
#	undef REMOVE_POLY_TAG

	if(mesh) {
		if(do_polyloop_free) {
			mesh_strip_loose_polysloops(mesh);
		}

		if (do_edge_free) {
			mesh_strip_loose_edges(mesh);
		}

		if(do_edge_recalc) {
			BKE_mesh_calc_edges(mesh, TRUE);
		}
	}

	return (verts_fixed || vert_weights_fixed || do_polyloop_free || do_edge_free || do_edge_recalc);
}

static int mesh_validate_customdata(CustomData *data, short do_verbose, const short do_fixes)
{
	int i= 0, has_fixes= 0;

	while(i<data->totlayer) {
		CustomDataLayer *layer= &data->layers[i];
		CustomDataMask mask= CD_TYPE_AS_MASK(layer->type);
		int ok= 1;

		if((mask&CD_MASK_MESH)==0) {
			PRINT("CustomDataLayer type %d which isn't in CD_MASK_MESH is stored in Mehs structure\n", layer->type);

			if(do_fixes) {
				CustomData_free_layer(data, layer->type, 0, i);
				ok= 0;
				has_fixes= 1;
			}
		}

		if(ok)
			i++;
	}

	return has_fixes;
}

#undef PRINT

static int BKE_mesh_validate_all_customdata(CustomData *vdata, CustomData *edata,
                                            CustomData *ldata, CustomData *pdata,
                                            short do_verbose, const short do_fixes)
{
	int vfixed= 0, efixed= 0, lfixed = 0, pfixed = 0;

	vfixed= mesh_validate_customdata(vdata, do_verbose, do_fixes);
	efixed= mesh_validate_customdata(edata, do_verbose, do_fixes);
	lfixed= mesh_validate_customdata(ldata, do_verbose, do_fixes);
	pfixed= mesh_validate_customdata(pdata, do_verbose, do_fixes);

	return vfixed || efixed || lfixed || pfixed;
}

int BKE_mesh_validate(Mesh *me, int do_verbose)
{
	int layers_fixed= 0, arrays_fixed= 0;

	if(do_verbose) {
		printf("MESH: %s\n", me->id.name+2);
	}

	layers_fixed= BKE_mesh_validate_all_customdata(&me->vdata, &me->edata, &me->ldata, &me->pdata, do_verbose, TRUE);
	arrays_fixed= BKE_mesh_validate_arrays(me,
	                                       me->mvert, me->totvert,
	                                       me->medge, me->totedge,
	                                       me->mloop, me->totloop,
	                                       me->mpoly, me->totpoly,
	                                       me->dvert,
	                                       do_verbose, TRUE);

	if (layers_fixed || arrays_fixed) {
		DAG_id_tag_update(&me->id, OB_RECALC_DATA);
		return TRUE;
	}
	return FALSE;
}

int BKE_mesh_validate_dm(DerivedMesh *dm)
{
	return BKE_mesh_validate_arrays(NULL,
	                                dm->getVertArray(dm), dm->getNumVerts(dm),
	                                dm->getEdgeArray(dm), dm->getNumEdges(dm),
	                                dm->getLoopArray(dm), dm->getNumLoops(dm),
	                                dm->getPolyArray(dm), dm->getNumPolys(dm),
	                                dm->getVertDataArray(dm, CD_MDEFORMVERT),
	                                TRUE, FALSE);
}

void BKE_mesh_calc_edges(Mesh *mesh, int update)
{
	CustomData edata;
	EdgeHashIterator *ehi;
	MPoly *mp = mesh->mpoly;
	MEdge *med, *med_orig;
	EdgeHash *eh = BLI_edgehash_new();
	int i, totedge, totpoly = mesh->totpoly;
	int med_index;

	if(mesh->totedge==0)
		update= 0;

	if(update) {
		/* assume existing edges are valid
		 * useful when adding more faces and generating edges from them */
		med= mesh->medge;
		for(i= 0; i<mesh->totedge; i++, med++)
			BLI_edgehash_insert(eh, med->v1, med->v2, med);
	}

	/* mesh loops (bmesh only) */
	for(i=0; i < totpoly; i++, mp++) {
		MLoop *l= &mesh->mloop[mp->loopstart];
		int j, l_prev= (l + (mp->totloop-1))->v;
		for (j=0; j < mp->totloop; j++, l++) {
			if (!BLI_edgehash_haskey(eh, l_prev, l->v)) {
				BLI_edgehash_insert(eh, l_prev, l->v, NULL);
			}
			l_prev= l->v;
		}
	}

	totedge = BLI_edgehash_size(eh);

	/* write new edges into a temporary CustomData */
	memset(&edata, 0, sizeof(edata));
	CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);

	ehi = BLI_edgehashIterator_new(eh);
	med = CustomData_get_layer(&edata, CD_MEDGE);
	for(i = 0; !BLI_edgehashIterator_isDone(ehi);
		BLI_edgehashIterator_step(ehi), ++i, ++med) {

		if(update && (med_orig=BLI_edgehashIterator_getValue(ehi))) {
			*med= *med_orig; /* copy from the original */
		} else {
			BLI_edgehashIterator_getKey(ehi, &med->v1, &med->v2);
			med->flag = ME_EDGEDRAW|ME_EDGERENDER|SELECT; /* select for newly created meshes which are selected [#25595] */
		}

		/* store the new edge index in the hash value */
		BLI_edgehashIterator_setValue(ehi, SET_INT_IN_POINTER(i));
	}
	BLI_edgehashIterator_free(ehi);

	if (mesh->totpoly) {
		/* second pass, iterate through all loops again and assign
		 * the newly created edges to them. */
		MPoly *mp= mesh->mpoly;
		for(i=0; i < mesh->totpoly; i++, mp++) {
			MLoop *l= &mesh->mloop[mp->loopstart];
			MLoop *l_prev= (l + (mp->totloop-1));
			int j;
			for (j=0; j < mp->totloop; j++, l++) {
				/* lookup hashed edge index */
				med_index = GET_INT_FROM_POINTER(BLI_edgehash_lookup(eh, l_prev->v, l->v));
				l_prev->e = med_index;
				l_prev= l;
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
