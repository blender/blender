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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_solidify.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_array.h"
#include "BLI_string.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"


typedef struct EdgeFaceRef {
	int f1; /* init as -1 */
	int f2;
} EdgeFaceRef;

static void dm_calc_normal(DerivedMesh *dm, float (*temp_nors)[3])
{
	int i, numVerts, numEdges, numFaces;
	MPoly *mpoly, *mp;
	MLoop *mloop, *ml;
	MVert *mvert, *mv;

	float (*face_nors)[3];
	float *f_no;
	int calc_face_nors = 0;

	numVerts = dm->getNumVerts(dm);
	numEdges = dm->getNumEdges(dm);
	numFaces = dm->getNumPolys(dm);
	mpoly = dm->getPolyArray(dm);
	mvert = dm->getVertArray(dm);
	mloop = dm->getLoopArray(dm);
	
	/* we don't want to overwrite any referenced layers */

	/* Doesn't work here! */
#if 0
	mv = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT, numVerts);
	cddm->mvert = mv;
#endif

	face_nors = CustomData_get_layer(&dm->polyData, CD_NORMAL);
	if (!face_nors) {
		calc_face_nors = 1;
		face_nors = CustomData_add_layer(&dm->polyData, CD_NORMAL, CD_CALLOC, NULL, numFaces);
	}

	mv = mvert;
	mp = mpoly;

	{
		EdgeHash *edge_hash = BLI_edgehash_new();
		EdgeHashIterator *edge_iter;
		int edge_ref_count = 0;
		unsigned int ed_v1, ed_v2; /* use when getting the key */
		EdgeFaceRef *edge_ref_array = MEM_callocN(numEdges * sizeof(EdgeFaceRef), "Edge Connectivity");
		EdgeFaceRef *edge_ref;
		float edge_normal[3];

		/* This function adds an edge hash if its not there, and adds the face index */
#define NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(EDV1, EDV2); \
			{ \
				const unsigned int ml_v1 = EDV1; \
				const unsigned int ml_v2 = EDV2; \
				edge_ref = (EdgeFaceRef *)BLI_edgehash_lookup(edge_hash, ml_v1, ml_v2); \
				if (!edge_ref) { \
					edge_ref = &edge_ref_array[edge_ref_count]; edge_ref_count++; \
					edge_ref->f1 = i; \
					edge_ref->f2 =- 1; \
					BLI_edgehash_insert(edge_hash, ml_v1, ml_v2, edge_ref); \
				} \
				else { \
					edge_ref->f2 = i; \
				} \
			}
		/* --- end define --- */

		for (i = 0; i < numFaces; i++, mp++) {
			int j;
			
			f_no = face_nors[i];
			if (calc_face_nors)
				mesh_calc_poly_normal(mp, mloop + mp->loopstart, mvert, f_no);

			ml = mloop + mp->loopstart;
			for (j = 0; j < mp->totloop; j++, ml++) {
				NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(ml->v, ME_POLY_LOOP_NEXT(mloop, mp, j)->v);
			}
		}

		for (edge_iter = BLI_edgehashIterator_new(edge_hash);
		     !BLI_edgehashIterator_isDone(edge_iter);
		     BLI_edgehashIterator_step(edge_iter))
		{
			/* Get the edge vert indices, and edge value (the face indices that use it)*/
			BLI_edgehashIterator_getKey(edge_iter, &ed_v1, &ed_v2);
			edge_ref = BLI_edgehashIterator_getValue(edge_iter);

			if (edge_ref->f2 != -1) {
				/* We have 2 faces using this edge, calculate the edges normal
				 * using the angle between the 2 faces as a weighting */
				add_v3_v3v3(edge_normal, face_nors[edge_ref->f1], face_nors[edge_ref->f2]);
				normalize_v3(edge_normal);
				mul_v3_fl(edge_normal, angle_normalized_v3v3(face_nors[edge_ref->f1], face_nors[edge_ref->f2]));
			}
			else {
				/* only one face attached to that edge */
				/* an edge without another attached- the weight on this is
				 * undefined, M_PI/2 is 90d in radians and that seems good enough */
				mul_v3_v3fl(edge_normal, face_nors[edge_ref->f1], M_PI / 2);
			}
			add_v3_v3(temp_nors[ed_v1], edge_normal);
			add_v3_v3(temp_nors[ed_v2], edge_normal);
		}
		BLI_edgehashIterator_free(edge_iter);
		BLI_edgehash_free(edge_hash, NULL);
		MEM_freeN(edge_ref_array);
	}

	/* normalize vertex normals and assign */
	for (i = 0; i < numVerts; i++, mv++) {
		if (normalize_v3(temp_nors[i]) == 0.0f) {
			normal_short_to_float_v3(temp_nors[i], mv->no);
		}
	}
}
 
static void initData(ModifierData *md)
{
	SolidifyModifierData *smd = (SolidifyModifierData *) md;
	smd->offset = 0.01f;
	smd->offset_fac = -1.0f;
	smd->flag = MOD_SOLIDIFY_RIM;
}
 
static void copyData(ModifierData *md, ModifierData *target)
{
	SolidifyModifierData *smd = (SolidifyModifierData *) md;
	SolidifyModifierData *tsmd = (SolidifyModifierData *) target;
	tsmd->offset = smd->offset;
	tsmd->offset_fac = smd->offset_fac;
	tsmd->crease_inner = smd->crease_inner;
	tsmd->crease_outer = smd->crease_outer;
	tsmd->crease_rim = smd->crease_rim;
	tsmd->flag = smd->flag;
	BLI_strncpy(tsmd->defgrp_name, smd->defgrp_name, sizeof(tsmd->defgrp_name));
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	SolidifyModifierData *smd = (SolidifyModifierData *) md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (smd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}


static DerivedMesh *applyModifier(ModifierData *md, Object *ob, 
                                  DerivedMesh *dm,
                                  ModifierApplyFlag UNUSED(flag))
{
	int i;
	DerivedMesh *result;
	const SolidifyModifierData *smd = (SolidifyModifierData *) md;

	MVert *mv, *mvert, *orig_mvert;
	MEdge *ed, *medge, *orig_medge;
	MLoop *ml, *mloop, *orig_mloop;
	MPoly *mp, *mpoly, *orig_mpoly;
	const int numVerts = dm->getNumVerts(dm);
	const int numEdges = dm->getNumEdges(dm);
	const int numFaces = dm->getNumPolys(dm);
	int numLoops = 0, newLoops = 0, newFaces = 0, newEdges = 0;
	int j;
	
	/* only use material offsets if we have 2 or more materials  */
	const short mat_nr_max = ob->totcol > 1 ? ob->totcol - 1 : 0;
	const short mat_ofs = mat_nr_max ? smd->mat_ofs : 0;
	const short mat_ofs_rim = mat_nr_max ? smd->mat_ofs_rim : 0;

	/* use for edges */
	int *new_vert_arr = NULL;
	BLI_array_declare(new_vert_arr);
	int *new_edge_arr = NULL;
	BLI_array_declare(new_edge_arr);
	int *old_vert_arr = MEM_callocN(sizeof(int) * numVerts, "old_vert_arr in solidify");

	int *edge_users = NULL;
	char *edge_order = NULL;
	int *edge_origIndex;
	
	float (*vert_nors)[3] = NULL;

	float (*face_nors_result)[3] = NULL;

	const float ofs_orig = -(((-smd->offset_fac + 1.0f) * 0.5f) * smd->offset);
	const float ofs_new  = smd->offset + ofs_orig;
	const float offset_fac_vg = smd->offset_fac_vg;
	const float offset_fac_vg_inv = 1.0f - smd->offset_fac_vg;

	/* weights */
	MDeformVert *dvert, *dv = NULL;
	const int defgrp_invert = ((smd->flag & MOD_SOLIDIFY_VGROUP_INV) != 0);
	int defgrp_index;

	modifier_get_vgroup(ob, dm, smd->defgrp_name, &dvert, &defgrp_index);

	numLoops = dm->numLoopData;
	newLoops = 0;
	
	orig_mvert = dm->getVertArray(dm);
	orig_medge = dm->getEdgeArray(dm);
	orig_mloop = dm->getLoopArray(dm);
	orig_mpoly = dm->getPolyArray(dm);

	if (smd->flag & MOD_SOLIDIFY_RIM) {
		EdgeHash *edgehash = BLI_edgehash_new();
		EdgeHashIterator *ehi;
		unsigned int v1, v2;
		int eidx;

#define INVALID_UNUSED -1
#define INVALID_PAIR -2

		edge_users = MEM_mallocN(sizeof(int) * numEdges, "solid_mod edges");
		edge_order = MEM_mallocN(sizeof(char) * numEdges, "solid_mod eorder");

		for (i = 0, mv = orig_mvert; i < numVerts; i++, mv++) {
			mv->flag &= ~ME_VERT_TMP_TAG;
		}

		/* save doing 2 loops here... */
#if 0
		fill_vn_i(edge_users, numEdges, INVALID_UNUSED);
#endif

		for (i = 0, ed = orig_medge; i < numEdges; i++, ed++) {
			BLI_edgehash_insert(edgehash, ed->v1, ed->v2, SET_INT_IN_POINTER(i));
			edge_users[i] = INVALID_UNUSED;
		}

		for (i = 0, mp = orig_mpoly; i < numFaces; i++, mp++) {
			MLoop *ml = orig_mloop + mp->loopstart;
			unsigned int ml_v1;
			unsigned int ml_v2;

			for (j = 0, ml_v1 = ml->v, ml_v2 = ml[mp->totloop - 1].v;
			     j < mp->totloop;
			     j++, ml++, ml_v2 = ml_v1, ml_v1 = ml->v)
			{
				/* add edge user */
				eidx = GET_INT_FROM_POINTER(BLI_edgehash_lookup(edgehash, ml_v1, ml_v2));
				if (edge_users[eidx] == INVALID_UNUSED) {
					ed = orig_medge + eidx;
					edge_users[eidx] = (ml_v1 < ml_v2) == (ed->v1 < ed->v2) ? i : (i + numFaces);
					edge_order[eidx] = j;
				}
				else {
					edge_users[eidx] = INVALID_PAIR;
				}
			}
		}

#undef ADD_EDGE_USER
#undef INVALID_UNUSED
#undef INVALID_PAIR

		ehi = BLI_edgehashIterator_new(edgehash);
		for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
			eidx = GET_INT_FROM_POINTER(BLI_edgehashIterator_getValue(ehi));
			if (edge_users[eidx] >= 0) {
				BLI_edgehashIterator_getKey(ehi, &v1, &v2);
				orig_mvert[v1].flag |= ME_VERT_TMP_TAG;
				orig_mvert[v2].flag |= ME_VERT_TMP_TAG;
				BLI_array_append(new_edge_arr, eidx);
				newFaces++;
				newLoops += 4;
			}
		}
		BLI_edgehashIterator_free(ehi);

		for (i = 0, mv = orig_mvert; i < numVerts; i++, mv++) {
			if (mv->flag & ME_VERT_TMP_TAG) {
				old_vert_arr[i] = BLI_array_count(new_vert_arr);
				BLI_array_append(new_vert_arr, i);
				newEdges++;

				mv->flag &= ~ME_VERT_TMP_TAG;
			}
		}

		BLI_edgehash_free(edgehash, NULL);
	}

	if (smd->flag & MOD_SOLIDIFY_NORMAL_CALC) {
		vert_nors = MEM_callocN(sizeof(float) * numVerts * 3, "mod_solid_vno_hq");
		dm_calc_normal(dm, vert_nors);
	}

	result = CDDM_from_template(dm, numVerts * 2, (numEdges * 2) + newEdges, 0,
	                            (numLoops * 2) + newLoops, (numFaces * 2) + newFaces);

	mpoly = CDDM_get_polys(result);
	mloop = CDDM_get_loops(result);
	medge = CDDM_get_edges(result);
	mvert = CDDM_get_verts(result);

	DM_copy_edge_data(dm, result, 0, 0, numEdges);
	DM_copy_edge_data(dm, result, 0, numEdges, numEdges);

	DM_copy_vert_data(dm, result, 0, 0, numVerts);
	DM_copy_vert_data(dm, result, 0, numVerts, numVerts);

	DM_copy_loop_data(dm, result, 0, 0, numLoops);
	DM_copy_loop_data(dm, result, 0, numLoops, numLoops);

	DM_copy_poly_data(dm, result, 0, 0, numFaces);
	DM_copy_poly_data(dm, result, 0, numFaces, numFaces);

	/* if the original has it, get the result so we can update it */
	face_nors_result = CustomData_get_layer(&result->polyData, CD_NORMAL);

	/*flip normals*/
	mp = mpoly + numFaces;
	for (i = 0; i < dm->numPolyData; i++, mp++) {
		MLoop *ml2;
		int e;

		ml2 = mloop + mp->loopstart + dm->numLoopData;
		for (j = 0; j < mp->totloop; j++) {
			CustomData_copy_data(&dm->loopData, &result->loopData, mp->loopstart + j,
			                     mp->loopstart + (mp->totloop - j - 1) + dm->numLoopData, 1);
		}

		if (mat_ofs) {
			mp->mat_nr += mat_ofs;
			CLAMP(mp->mat_nr, 0, mat_nr_max);
		}

		e = ml2[0].e;
		for (j = 0; j < mp->totloop - 1; j++) {
			ml2[j].e = ml2[j + 1].e;
		}
		ml2[mp->totloop - 1].e = e;
		
		mp->loopstart += dm->numLoopData;
		
		for (j = 0; j < mp->totloop; j++) {
			ml2[j].e += numEdges;
			ml2[j].v += numVerts;
		}

		if (face_nors_result) {
			negate_v3_v3(face_nors_result[numFaces + i], face_nors_result[i]);
		}
	}

	for (i = 0, ed = medge + numEdges; i < numEdges; i++, ed++) {
		ed->v1 += numVerts;
		ed->v2 += numVerts;
	}

	/* note, copied vertex layers don't have flipped normals yet. do this after applying offset */
	if ((smd->flag & MOD_SOLIDIFY_EVEN) == 0) {
		/* no even thickness, very simple */
		float scalar_short;
		float scalar_short_vgroup;


		if (ofs_new != 0.0f) {
			scalar_short = scalar_short_vgroup = ofs_new / 32767.0f;
			mv = mvert + ((ofs_new >= ofs_orig) ? 0 : numVerts);
			dv = dvert;
			for (i = 0; i < numVerts; i++, mv++) {
				if (dv) {
					if (defgrp_invert) scalar_short_vgroup = 1.0f - defvert_find_weight(dv, defgrp_index);
					else scalar_short_vgroup = defvert_find_weight(dv, defgrp_index);
					scalar_short_vgroup = (offset_fac_vg + (scalar_short_vgroup * offset_fac_vg_inv)) * scalar_short;
					dv++;
				}
				VECADDFAC(mv->co, mv->co, mv->no, scalar_short_vgroup);
			}
		}

		if (ofs_orig != 0.0f) {
			scalar_short = scalar_short_vgroup = ofs_orig / 32767.0f;
			mv = mvert + ((ofs_new >= ofs_orig) ? numVerts : 0); /* as above but swapped, intentional use 'ofs_new' */
			dv = dvert;
			for (i = 0; i < numVerts; i++, mv++) {
				if (dv) {
					if (defgrp_invert) scalar_short_vgroup = 1.0f - defvert_find_weight(dv, defgrp_index);
					else scalar_short_vgroup = defvert_find_weight(dv, defgrp_index);
					scalar_short_vgroup = (offset_fac_vg + (scalar_short_vgroup * offset_fac_vg_inv)) * scalar_short;
					dv++;
				}
				VECADDFAC(mv->co, mv->co, mv->no, scalar_short_vgroup);
			}
		}

	}
	else {
		/* make a face normal layer if not present */
		float (*face_nors)[3];
		int face_nors_calc = 0;

		/* same as EM_solidify() in editmesh_lib.c */
		float *vert_angles = MEM_callocN(sizeof(float) * numVerts * 2, "mod_solid_pair"); /* 2 in 1 */
		float *vert_accum = vert_angles + numVerts;
		int vidx;

		face_nors = CustomData_get_layer(&dm->polyData, CD_NORMAL);
		if (!face_nors) {
			face_nors = CustomData_add_layer(&dm->polyData, CD_NORMAL, CD_CALLOC, NULL, dm->numPolyData);
			face_nors_calc = 1;
		}

		if (vert_nors == NULL) {
			vert_nors = MEM_mallocN(sizeof(float) * numVerts * 3, "mod_solid_vno");
			for (i = 0, mv = mvert; i < numVerts; i++, mv++) {
				normal_short_to_float_v3(vert_nors[i], mv->no);
			}
		}

		for (i = 0, mp = mpoly; i < numFaces; i++, mp++) {
			/* #BKE_mesh_poly_calc_angles logic is inlined here */
			float nor_prev[3];
			float nor_next[3];

			int i_this = mp->totloop - 1;
			int i_next = 0;

			ml = &mloop[mp->loopstart];

			/* --- not related to angle calc --- */
			if (face_nors_calc)
				mesh_calc_poly_normal(mp, ml, mvert, face_nors[i]);
			/* --- end non-angle-calc section --- */

			sub_v3_v3v3(nor_prev, mvert[ml[i_this - 1].v].co, mvert[ml[i_this].v].co);
			normalize_v3(nor_prev);

			while (i_next < mp->totloop) {
				float angle;
				sub_v3_v3v3(nor_next, mvert[ml[i_this].v].co, mvert[ml[i_next].v].co);
				normalize_v3(nor_next);
				angle = angle_normalized_v3v3(nor_prev, nor_next);


				/* --- not related to angle calc --- */
				if (angle < FLT_EPSILON) {
					angle = FLT_EPSILON;
				}
				vidx = ml[i_this].v;
				vert_accum[vidx] += angle;
				vert_angles[vidx] += shell_angle_to_dist(angle_normalized_v3v3(vert_nors[vidx], face_nors[i])) * angle;
				/* --- end non-angle-calc section --- */


				/* step */
				copy_v3_v3(nor_prev, nor_next);
				i_this = i_next;
				i_next++;
			}
		}

		/* vertex group support */
		if (dvert) {
			float scalar;

			dv = dvert;
			if (defgrp_invert) {
				for (i = 0; i < numVerts; i++, dv++) {
					scalar = 1.0f - defvert_find_weight(dv, defgrp_index);
					scalar = offset_fac_vg + (scalar * offset_fac_vg_inv);
					vert_angles[i] *= scalar;
				}
			}
			else {
				for (i = 0; i < numVerts; i++, dv++) {
					scalar = defvert_find_weight(dv, defgrp_index);
					scalar = offset_fac_vg + (scalar * offset_fac_vg_inv);
					vert_angles[i] *= scalar;
				}
			}
		}

		if (ofs_new) {
			mv = mvert + ((ofs_new >= ofs_orig) ? 0 : numVerts);

			for (i = 0; i < numVerts; i++, mv++) {
				if (vert_accum[i]) { /* zero if unselected */
					madd_v3_v3fl(mv->co, vert_nors[i], ofs_new * (vert_angles[i] / vert_accum[i]));
				}
			}
		}

		if (ofs_orig) {
			/* same as above but swapped, intentional use of 'ofs_new' */
			mv = mvert + ((ofs_new >= ofs_orig) ? numVerts : 0);

			for (i = 0; i < numVerts; i++, mv++) {
				if (vert_accum[i]) { /* zero if unselected */
					madd_v3_v3fl(mv->co, vert_nors[i], ofs_orig * (vert_angles[i] / vert_accum[i]));
				}
			}
		}

		MEM_freeN(vert_angles);
	}

	if (vert_nors)
		MEM_freeN(vert_nors);

	/* flip vertex normals for copied verts */
	mv = mvert + numVerts;
	for (i = 0; i < numVerts; i++, mv++) {
		negate_v3_short(mv->no);
	}

	if (smd->flag & MOD_SOLIDIFY_RIM) {
		int *origindex;
		
		/* bugger, need to re-calculate the normals for the new edge faces.
		 * This could be done in many ways, but probably the quickest way
		 * is to calculate the average normals for side faces only.
		 * Then blend them with the normals of the edge verts.
		 * 
		 * at the moment its easiest to allocate an entire array for every vertex,
		 * even though we only need edge verts - campbell
		 */
		
#define SOLIDIFY_SIDE_NORMALS

#ifdef SOLIDIFY_SIDE_NORMALS
		/* annoying to allocate these since we only need the edge verts, */
		float (*edge_vert_nos)[3] = MEM_callocN(sizeof(float) * numVerts * 3, "solidify_edge_nos");
		float nor[3];
#endif
		const unsigned char crease_rim = smd->crease_rim * 255.0f;
		const unsigned char crease_outer = smd->crease_outer * 255.0f;
		const unsigned char crease_inner = smd->crease_inner * 255.0f;

		/* add faces & edges */
		origindex = result->getEdgeDataArray(result, CD_ORIGINDEX);
		ed = medge + (numEdges * 2);
		for (i = 0; i < newEdges; i++, ed++) {
			ed->v1 = new_vert_arr[i];
			ed->v2 = new_vert_arr[i] + numVerts;
			ed->flag |= ME_EDGEDRAW;

			origindex[numEdges * 2 + i] = ORIGINDEX_NONE;

			if (crease_rim)
				ed->crease = crease_rim;
		}

		/* faces */
		edge_origIndex = origindex;
		origindex = DM_get_poly_data_layer(result, CD_ORIGINDEX);
		
		mp = mpoly + (numFaces * 2);
		ml = mloop + (numLoops * 2);
		j = 0;
		for (i = 0; i < newFaces; i++, mp++) {
			int eidx = new_edge_arr[i];
			int fidx = edge_users[eidx];
			int flip, k1, k2;

			if (fidx >= numFaces) {
				fidx -= numFaces;
				flip = 1;
			}
			else {
				flip = 0;
			}

			ed = medge + eidx;

			/* copy most of the face settings */
			DM_copy_poly_data(dm, result, fidx, (numFaces * 2) + i, 1);
			mp->loopstart = j + numLoops * 2;
			mp->flag = mpoly[fidx].flag;

			/* notice we use 'mp->totloop' which is later overwritten,
			 * we could lookup the original face but theres no point since this is a copy
			 * and will have the same value, just take care when changing order of assignment */
			k1 = mpoly[fidx].loopstart + ((edge_order[eidx] + 1) % mp->totloop);
			k2 = mpoly[fidx].loopstart +  (edge_order[eidx]);

			mp->totloop = 4;

			CustomData_copy_data(&dm->loopData, &result->loopData, k1, numLoops * 2 + j + 0, 1);
			CustomData_copy_data(&dm->loopData, &result->loopData, k2, numLoops * 2 + j + 1, 1);
			CustomData_copy_data(&dm->loopData, &result->loopData, k2, numLoops * 2 + j + 2, 1);
			CustomData_copy_data(&dm->loopData, &result->loopData, k1, numLoops * 2 + j + 3, 1);

			if (flip) {
				ml[j].v = ed->v1;
				ml[j++].e = eidx;
				
				ml[j].v = ed->v2;
				ml[j++].e = numEdges * 2 + old_vert_arr[ed->v2];
				
				ml[j].v = ed->v2 + numVerts;
				ml[j++].e = eidx + numEdges;
				
				ml[j].v = ed->v1 + numVerts;
				ml[j++].e = numEdges * 2 + old_vert_arr[ed->v1];
			}
			else {
				ml[j].v = ed->v2;
				ml[j++].e = eidx;

				ml[j].v = ed->v1;
				ml[j++].e = numEdges * 2 + old_vert_arr[ed->v1];

				ml[j].v = ed->v1 + numVerts;
				ml[j++].e = eidx + numEdges;

				ml[j].v = ed->v2 + numVerts;
				ml[j++].e = numEdges * 2 + old_vert_arr[ed->v2];
			}
			
			if (edge_origIndex) {
				edge_origIndex[ml[j - 3].e] = ORIGINDEX_NONE;
				edge_origIndex[ml[j - 1].e] = ORIGINDEX_NONE;
			}

			/* use the next material index if option enabled */
			if (mat_ofs_rim) {
				mp->mat_nr += mat_ofs_rim;
				CLAMP(mp->mat_nr, 0, mat_nr_max);
			}
			if (crease_outer) {
				/* crease += crease_outer; without wrapping */
				unsigned char *cr = (unsigned char *)&(ed->crease);
				int tcr = *cr + crease_outer;
				*cr = tcr > 255 ? 255 : tcr;
			}

			if (crease_inner) {
				/* crease += crease_inner; without wrapping */
				unsigned char *cr = (unsigned char *)&(medge[numEdges + eidx].crease);
				int tcr = *cr + crease_inner;
				*cr = tcr > 255 ? 255 : tcr;
			}
			
#ifdef SOLIDIFY_SIDE_NORMALS
			normal_quad_v3(nor,
			               mvert[ml[j - 4].v].co,
			               mvert[ml[j - 3].v].co,
			               mvert[ml[j - 2].v].co,
			               mvert[ml[j - 1].v].co);

			add_v3_v3(edge_vert_nos[ed->v1], nor);
			add_v3_v3(edge_vert_nos[ed->v2], nor);

			if (face_nors_result) {
				copy_v3_v3(face_nors_result[(numFaces * 2) + i], nor);
			}
#endif
		}
		
#ifdef SOLIDIFY_SIDE_NORMALS
		ed = medge + (numEdges * 2);
		for (i = 0; i < newEdges; i++, ed++) {
			float nor_cpy[3];
			short *nor_short;
			int j;
			
			/* note, only the first vertex (lower half of the index) is calculated */
			normalize_v3_v3(nor_cpy, edge_vert_nos[ed->v1]);
			
			for (j = 0; j < 2; j++) { /* loop over both verts of the edge */
				nor_short = mvert[*(&ed->v1 + j)].no;
				normal_short_to_float_v3(nor, nor_short);
				add_v3_v3(nor, nor_cpy);
				normalize_v3(nor);
				normal_float_to_short_v3(nor_short, nor);
			}
		}

		MEM_freeN(edge_vert_nos);
#endif

		BLI_array_free(new_vert_arr);
		BLI_array_free(new_edge_arr);
		MEM_freeN(edge_users);
		MEM_freeN(edge_order);
	}

	if (old_vert_arr)
		MEM_freeN(old_vert_arr);
	
	/* must recalculate normals with vgroups since they can displace unevenly [#26888] */
	if (dvert) {
		CDDM_calc_normals(result);
	}

	return result;
}

#undef SOLIDIFY_SIDE_NORMALS

static DerivedMesh *applyModifierEM(ModifierData *md,
                                    Object *ob,
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, MOD_APPLY_USECACHE);
}


ModifierTypeInfo modifierType_Solidify = {
	/* name */              "Solidify",
	/* structName */        "SolidifyModifierData",
	/* structSize */        sizeof(SolidifyModifierData),
	/* type */              eModifierTypeType_Constructive,

	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
