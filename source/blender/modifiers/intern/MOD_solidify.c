/*
* $Id$
*
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

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_edgehash.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"
#include "BKE_utildefines.h"

#include "MOD_modifiertypes.h"

#include "MEM_guardedalloc.h"

typedef struct EdgeFaceRef {
	int f1; /* init as -1 */
	int f2;
} EdgeFaceRef;

static void dm_calc_normal(DerivedMesh *dm, float (*temp_nors)[3])
{
	int i, numVerts, numEdges, numFaces;
	MFace *mface, *mf;
	MVert *mvert, *mv;

	float (*face_nors)[3];
	float *f_no;
	int calc_face_nors= 0;

	numVerts = dm->getNumVerts(dm);
	numEdges = dm->getNumEdges(dm);
	numFaces = dm->getNumFaces(dm);
	mface = dm->getFaceArray(dm);
	mvert = dm->getVertArray(dm);

	/* we don't want to overwrite any referenced layers */

	/*
	Dosnt work here!
	mv = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MVERT);
	cddm->mvert = mv;
	*/

	face_nors = CustomData_get_layer(&dm->faceData, CD_NORMAL);
	if(!face_nors) {
		calc_face_nors = 1;
		face_nors = CustomData_add_layer(&dm->faceData, CD_NORMAL, CD_CALLOC, NULL, numFaces);
	}

	mv = mvert;
	mf = mface;

	{
		EdgeHash *edge_hash = BLI_edgehash_new();
		EdgeHashIterator *edge_iter;
		int edge_ref_count = 0;
		int ed_v1, ed_v2; /* use when getting the key */
		EdgeFaceRef *edge_ref_array = MEM_callocN(numEdges * sizeof(EdgeFaceRef), "Edge Connectivity");
		EdgeFaceRef *edge_ref;
		float edge_normal[3];

		/* This function adds an edge hash if its not there, and adds the face index */
#define NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(EDV1, EDV2); \
				edge_ref = (EdgeFaceRef *)BLI_edgehash_lookup(edge_hash, EDV1, EDV2); \
				if (!edge_ref) { \
					edge_ref = &edge_ref_array[edge_ref_count]; edge_ref_count++; \
					edge_ref->f1=i; \
					edge_ref->f2=-1; \
					BLI_edgehash_insert(edge_hash, EDV1, EDV2, edge_ref); \
				} else { \
					edge_ref->f2=i; \
				}

		for(i = 0; i < numFaces; i++, mf++) {
			f_no = face_nors[i];

			if(mf->v4) {
				if(calc_face_nors)
					normal_quad_v3(f_no, mv[mf->v1].co, mv[mf->v2].co, mv[mf->v3].co, mv[mf->v4].co);

				NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(mf->v1, mf->v2);
				NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(mf->v2, mf->v3);
				NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(mf->v3, mf->v4);
				NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(mf->v4, mf->v1);
			} else {
				if(calc_face_nors)
					normal_tri_v3(f_no, mv[mf->v1].co, mv[mf->v2].co, mv[mf->v3].co);

				NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(mf->v1, mf->v2);
				NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(mf->v2, mf->v3);
				NOCALC_EDGEWEIGHT_ADD_EDGEREF_FACE(mf->v3, mf->v1);
			}
		}

		for(edge_iter = BLI_edgehashIterator_new(edge_hash); !BLI_edgehashIterator_isDone(edge_iter); BLI_edgehashIterator_step(edge_iter)) {
			/* Get the edge vert indicies, and edge value (the face indicies that use it)*/
			BLI_edgehashIterator_getKey(edge_iter, (int*)&ed_v1, (int*)&ed_v2);
			edge_ref = BLI_edgehashIterator_getValue(edge_iter);

			if (edge_ref->f2 != -1) {
				/* We have 2 faces using this edge, calculate the edges normal
				 * using the angle between the 2 faces as a weighting */
				add_v3_v3v3(edge_normal, face_nors[edge_ref->f1], face_nors[edge_ref->f2]);
				normalize_v3(edge_normal);
				mul_v3_fl(edge_normal, angle_normalized_v3v3(face_nors[edge_ref->f1], face_nors[edge_ref->f2]));
			} else {
				/* only one face attached to that edge */
				/* an edge without another attached- the weight on this is
				 * undefined, M_PI/2 is 90d in radians and that seems good enough */
				mul_v3_v3fl(edge_normal, face_nors[edge_ref->f1], M_PI/2);
			}
			add_v3_v3(temp_nors[ed_v1], edge_normal);
			add_v3_v3(temp_nors[ed_v2], edge_normal);
		}
		BLI_edgehashIterator_free(edge_iter);
		BLI_edgehash_free(edge_hash, NULL);
		MEM_freeN(edge_ref_array);
	}

	/* normalize vertex normals and assign */
	for(i = 0; i < numVerts; i++, mv++) {
		if(normalize_v3(temp_nors[i]) == 0.0f) {
			normal_short_to_float_v3(temp_nors[i], mv->no);
		}
	}
}
 
static void initData(ModifierData *md)
{
	SolidifyModifierData *smd = (SolidifyModifierData*) md;
	smd->offset = 0.01f;
	smd->flag = MOD_SOLIDIFY_RIM;
}
 
static void copyData(ModifierData *md, ModifierData *target)
{
	SolidifyModifierData *smd = (SolidifyModifierData*) md;
	SolidifyModifierData *tsmd = (SolidifyModifierData*) target;
	tsmd->offset = smd->offset;
	tsmd->offset_fac = smd->offset_fac;
	tsmd->crease_inner = smd->crease_inner;
	tsmd->crease_outer = smd->crease_outer;
	tsmd->crease_rim = smd->crease_rim;
	tsmd->flag = smd->flag;
	strcpy(tsmd->defgrp_name, smd->defgrp_name);
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	SolidifyModifierData *smd = (SolidifyModifierData*) md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(smd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}


static DerivedMesh *applyModifier(ModifierData *md,
						   Object *ob, 
						   DerivedMesh *dm,
						   int useRenderParams,
						   int isFinalCalc)
{
	int i;
	DerivedMesh *result;
	SolidifyModifierData *smd = (SolidifyModifierData*) md;

	MFace *mf, *mface, *orig_mface;
	MEdge *ed, *medge, *orig_medge;
	MVert *mv, *mvert, *orig_mvert;

	int numVerts = dm->getNumVerts(dm);
	int numEdges = dm->getNumEdges(dm);
	int numFaces = dm->getNumFaces(dm);

	/* use for edges */
	int *new_vert_arr= NULL;
	int newFaces = 0;

	int *new_edge_arr= NULL;
	int newEdges = 0;

	int *edge_users= NULL;
	char *edge_order= NULL;

	float (*vert_nors)[3]= NULL;

	float ofs_orig=				- (((-smd->offset_fac + 1.0f) * 0.5f) * smd->offset);
	float ofs_new= smd->offset	- (((-smd->offset_fac + 1.0f) * 0.5f) * smd->offset);

	/* weights */
	MDeformVert *dvert= NULL, *dv= NULL;
	int defgrp_invert = ((smd->flag & MOD_SOLIDIFY_VGROUP_INV) != 0);
	int defgrp_index= defgroup_name_index(ob, smd->defgrp_name);

	if (defgrp_index >= 0)
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

	orig_mface = dm->getFaceArray(dm);
	orig_medge = dm->getEdgeArray(dm);
	orig_mvert = dm->getVertArray(dm);

	if(smd->flag & MOD_SOLIDIFY_RIM) {
		EdgeHash *edgehash = BLI_edgehash_new();
		EdgeHashIterator *ehi;
		int v1, v2;
		int eidx;

		for(i=0, mv=orig_mvert; i<numVerts; i++, mv++) {
			mv->flag &= ~ME_VERT_TMP_TAG;
		}

		for(i=0, ed=orig_medge; i<numEdges; i++, ed++) {
			BLI_edgehash_insert(edgehash, ed->v1, ed->v2, SET_INT_IN_POINTER(i));
		}

#define INVALID_UNUSED -1
#define INVALID_PAIR -2

#define ADD_EDGE_USER(_v1, _v2, edge_ord) \
		eidx= GET_INT_FROM_POINTER(BLI_edgehash_lookup(edgehash, _v1, _v2)); \
		if(edge_users[eidx] == INVALID_UNUSED) { \
			ed= orig_medge + eidx; \
			edge_users[eidx]= (_v1 < _v2) == (ed->v1 < ed->v2) ? i:(i+numFaces); \
			edge_order[eidx]= edge_ord; \
		} else { \
			edge_users[eidx]= INVALID_PAIR; \
		} \


		edge_users= MEM_mallocN(sizeof(int) * numEdges, "solid_mod edges");
		edge_order= MEM_mallocN(sizeof(char) * numEdges, "solid_mod eorder");
		memset(edge_users, INVALID_UNUSED, sizeof(int) * numEdges);

		for(i=0, mf=orig_mface; i<numFaces; i++, mf++) {
			if(mf->v4) {
				ADD_EDGE_USER(mf->v1, mf->v2, 0);
				ADD_EDGE_USER(mf->v2, mf->v3, 1);
				ADD_EDGE_USER(mf->v3, mf->v4, 2);
				ADD_EDGE_USER(mf->v4, mf->v1, 3);
			}
			else {
				ADD_EDGE_USER(mf->v1, mf->v2, 0);
				ADD_EDGE_USER(mf->v2, mf->v3, 1);
				ADD_EDGE_USER(mf->v3, mf->v1, 2);
			}
		}

#undef ADD_EDGE_USER
#undef INVALID_UNUSED
#undef INVALID_PAIR


		new_edge_arr= MEM_callocN(sizeof(int) * numEdges, "solid_mod arr");

		ehi= BLI_edgehashIterator_new(edgehash);
		for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
			int eidx= GET_INT_FROM_POINTER(BLI_edgehashIterator_getValue(ehi));
			if(edge_users[eidx] >= 0) {
				BLI_edgehashIterator_getKey(ehi, &v1, &v2);
				orig_mvert[v1].flag |= ME_VERT_TMP_TAG;
				orig_mvert[v2].flag |= ME_VERT_TMP_TAG;
				new_edge_arr[newFaces]= eidx;
				newFaces++;
			}
		}
		BLI_edgehashIterator_free(ehi);



		new_vert_arr= MEM_callocN(sizeof(int) * numVerts, "solid_mod new_varr");
		for(i=0, mv=orig_mvert; i<numVerts; i++, mv++) {
			if(mv->flag & ME_VERT_TMP_TAG) {
				new_vert_arr[newEdges] = i;
				newEdges++;

				mv->flag &= ~ME_VERT_TMP_TAG;
			}
		}

		BLI_edgehash_free(edgehash, NULL);
	}

	if(smd->flag & MOD_SOLIDIFY_NORMAL_CALC) {
		vert_nors= MEM_callocN(sizeof(float) * numVerts * 3, "mod_solid_vno_hq");
		dm_calc_normal(dm, vert_nors);
	}

	result = CDDM_from_template(dm, numVerts * 2, (numEdges * 2) + newEdges, (numFaces * 2) + newFaces);

	mface = result->getFaceArray(result);
	medge = result->getEdgeArray(result);
	mvert = result->getVertArray(result);

	DM_copy_face_data(dm, result, 0, 0, numFaces);
	DM_copy_face_data(dm, result, 0, numFaces, numFaces);

	DM_copy_edge_data(dm, result, 0, 0, numEdges);
	DM_copy_edge_data(dm, result, 0, numEdges, numEdges);

	DM_copy_vert_data(dm, result, 0, 0, numVerts);
	DM_copy_vert_data(dm, result, 0, numVerts, numVerts);

	{
		static int corner_indices[4] = {2, 1, 0, 3};
		int is_quad;

		for(i=0, mf=mface+numFaces; i<numFaces; i++, mf++) {
			mf->v1 += numVerts;
			mf->v2 += numVerts;
			mf->v3 += numVerts;
			if(mf->v4)
				mf->v4 += numVerts;

			/* Flip face normal */
			{
				is_quad = mf->v4;
				SWAP(int, mf->v1, mf->v3);
				DM_swap_face_data(result, i+numFaces, corner_indices);
				test_index_face(mf, &result->faceData, numFaces, is_quad ? 4:3);
			}
		}
	}

	for(i=0, ed=medge+numEdges; i<numEdges; i++, ed++) {
		ed->v1 += numVerts;
		ed->v2 += numVerts;
	}

	/* note, copied vertex layers dont have flipped normals yet. do this after applying offset */
	if((smd->flag & MOD_SOLIDIFY_EVEN) == 0) {
		/* no even thickness, very simple */
		float scalar_short;
		float scalar_short_vgroup;


		if(ofs_new != 0.0f) {
			scalar_short= scalar_short_vgroup= ofs_new / 32767.0f;
			mv= mvert + ((ofs_new >= ofs_orig) ? 0 : numVerts);
			dv= dvert;
			for(i=0; i<numVerts; i++, mv++) {
				if(dv) {
					if(defgrp_invert)	scalar_short_vgroup = scalar_short * (1.0f - defvert_find_weight(dv, defgrp_index));
					else				scalar_short_vgroup = scalar_short * defvert_find_weight(dv, defgrp_index);
					dv++;
				}
				VECADDFAC(mv->co, mv->co, mv->no, scalar_short_vgroup);
			}
		}

		if(ofs_orig != 0.0f) {
			scalar_short= scalar_short_vgroup= ofs_orig / 32767.0f;
			mv= mvert + ((ofs_new >= ofs_orig) ? numVerts : 0); /* same as above but swapped, intentional use of 'ofs_new' */
			dv= dvert;
			for(i=0; i<numVerts; i++, mv++) {
				if(dv) {
					if(defgrp_invert)	scalar_short_vgroup = scalar_short * (1.0f - defvert_find_weight(dv, defgrp_index));
					else				scalar_short_vgroup = scalar_short * defvert_find_weight(dv, defgrp_index);
					dv++;
				}
				VECADDFAC(mv->co, mv->co, mv->no, scalar_short_vgroup);
			}
		}

	}
	else {
		/* make a face normal layer if not present */
		float (*face_nors)[3];
		int face_nors_calc= 0;

		/* same as EM_solidify() in editmesh_lib.c */
		float *vert_angles= MEM_callocN(sizeof(float) * numVerts * 2, "mod_solid_pair"); /* 2 in 1 */
		float *vert_accum= vert_angles + numVerts;
		float face_angles[4];
		int i, j, vidx;

		face_nors = CustomData_get_layer(&dm->faceData, CD_NORMAL);
		if(!face_nors) {
			face_nors = CustomData_add_layer(&dm->faceData, CD_NORMAL, CD_CALLOC, NULL, dm->numFaceData);
			face_nors_calc= 1;
		}

		if(vert_nors==NULL) {
			vert_nors= MEM_mallocN(sizeof(float) * numVerts * 3, "mod_solid_vno");
			for(i=0, mv=mvert; i<numVerts; i++, mv++) {
				normal_short_to_float_v3(vert_nors[i], mv->no);
			}
		}

		for(i=0, mf=mface; i<numFaces; i++, mf++) {

			/* just added, calc the normal */
			if(face_nors_calc) {
				if(mf->v4)
					normal_quad_v3(face_nors[i], mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co, mvert[mf->v4].co);
				else
					normal_tri_v3(face_nors[i] , mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co);
			}

			if(mf->v4) {
				angle_quad_v3(face_angles, mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co, mvert[mf->v4].co);
				j= 3;
			}
			else {
				angle_tri_v3(face_angles, mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co);
				j= 2;
			}

			for(; j>=0; j--) {
				vidx = *(&mf->v1 + j);
				vert_accum[vidx] += face_angles[j];
				vert_angles[vidx]+= shell_angle_to_dist(angle_normalized_v3v3(vert_nors[vidx], face_nors[i])) * face_angles[j];
			}
		}

		/* vertex group support */
		if(dvert) {
			dv= dvert;
			if(defgrp_invert) {
				for(i=0; i<numVerts; i++, dv++) {
					vert_angles[i] *= (1.0f - defvert_find_weight(dv, defgrp_index));
				}
			}
			else {
				for(i=0; i<numVerts; i++, dv++) {
					vert_angles[i] *= defvert_find_weight(dv, defgrp_index);
				}
			}
		}

		if(ofs_new) {
			mv= mvert + ((ofs_new >= ofs_orig) ? 0 : numVerts);

			for(i=0; i<numVerts; i++, mv++) {
				if(vert_accum[i]) { /* zero if unselected */
					madd_v3_v3fl(mv->co, vert_nors[i], ofs_new * (vert_angles[i] / vert_accum[i]));
				}
			}
		}

		if(ofs_orig) {
			mv= mvert + ((ofs_new >= ofs_orig) ? numVerts : 0); /* same as above but swapped, intentional use of 'ofs_new' */

			for(i=0; i<numVerts; i++, mv++) {
				if(vert_accum[i]) { /* zero if unselected */
					madd_v3_v3fl(mv->co, vert_nors[i], ofs_orig * (vert_angles[i] / vert_accum[i]));
				}
			}
		}

		MEM_freeN(vert_angles);
	}

	if(vert_nors)
		MEM_freeN(vert_nors);

	/* flip vertex normals for copied verts */
	mv= mvert + numVerts;
	for(i=0; i<numVerts; i++, mv++) {
		mv->no[0]= -mv->no[0];
		mv->no[1]= -mv->no[1];
		mv->no[2]= -mv->no[2];
	}

	if(smd->flag & MOD_SOLIDIFY_RIM) {

		
		/* bugger, need to re-calculate the normals for the new edge faces.
		 * This could be done in many ways, but probably the quickest way is to calculate the average normals for side faces only.
		 * Then blend them with the normals of the edge verts.
		 * 
		 * at the moment its easiest to allocate an entire array for every vertex, even though we only need edge verts - campbell
		 */
		
#define SOLIDIFY_SIDE_NORMALS

#ifdef SOLIDIFY_SIDE_NORMALS
		/* annoying to allocate these since we only need the edge verts, */
		float (*edge_vert_nos)[3]= MEM_callocN(sizeof(float) * numVerts * 3, "solidify_edge_nos");
		float nor[3];
#endif

		const unsigned char crease_rim= smd->crease_rim * 255.0f;
		const unsigned char crease_outer= smd->crease_outer * 255.0f;
		const unsigned char crease_inner= smd->crease_inner * 255.0f;

		const int edge_indices[4][4] = {
				{1, 0, 0, 1},
				{2, 1, 1, 2},
				{3, 2, 2, 3},
				{0, 3, 3, 0}};

		/* add faces & edges */
		ed= medge + (numEdges * 2);
		for(i=0; i<newEdges; i++, ed++) {
			ed->v1= new_vert_arr[i];
			ed->v2= new_vert_arr[i] + numVerts;
			ed->flag |= ME_EDGEDRAW;

			if(crease_rim)
				ed->crease= crease_rim;
		}

		/* faces */
		mf= mface + (numFaces * 2);
		for(i=0; i<newFaces; i++, mf++) {
			int eidx= new_edge_arr[i];
			int fidx= edge_users[eidx];
			int flip;

			if(fidx >= numFaces) {
				fidx -= numFaces;
				flip= 1;
			}
			else {
				flip= 0;
			}

			ed= medge + eidx;

			/* copy most of the face settings */
			DM_copy_face_data(dm, result, fidx, (numFaces * 2) + i, 1);

			if(flip) {
				DM_swap_face_data(result, (numFaces * 2) + i, edge_indices[edge_order[eidx]]);

				mf->v1= ed->v1;
				mf->v2= ed->v2;
				mf->v3= ed->v2 + numVerts;
				mf->v4= ed->v1 + numVerts;
			}
			else {
				DM_swap_face_data(result, (numFaces * 2) + i, edge_indices[edge_order[eidx]]);

				mf->v1= ed->v2;
				mf->v2= ed->v1;
				mf->v3= ed->v1 + numVerts;
				mf->v4= ed->v2 + numVerts;
			}

			if(crease_outer)
				ed->crease= crease_outer;

			if(crease_inner) {
				medge[numEdges + eidx].crease= crease_inner;
			}
			
#ifdef SOLIDIFY_SIDE_NORMALS
			normal_quad_v3(nor, mvert[mf->v1].co, mvert[mf->v2].co, mvert[mf->v3].co, mvert[mf->v4].co);

			add_v3_v3(edge_vert_nos[ed->v1], nor);
			add_v3_v3(edge_vert_nos[ed->v2], nor);
#endif
		}
		
#ifdef SOLIDIFY_SIDE_NORMALS
		ed= medge + (numEdges * 2);
		for(i=0; i<newEdges; i++, ed++) {
			float nor_cpy[3];
			short *nor_short;
			int j;
			
			/* note, only the first vertex (lower half of the index) is calculated */
			normalize_v3_v3(nor_cpy, edge_vert_nos[ed->v1]);
			
			for(j=0; j<2; j++) { /* loop over both verts of the edge */
				nor_short= mvert[*(&ed->v1 + j)].no;
				normal_short_to_float_v3(nor, nor_short);
				add_v3_v3(nor, nor_cpy);
				normalize_v3(nor);
				normal_float_to_short_v3(nor_short, nor);
			}
		}

		MEM_freeN(edge_vert_nos);
#endif

		MEM_freeN(new_vert_arr);
		MEM_freeN(new_edge_arr);
		MEM_freeN(edge_users);
		MEM_freeN(edge_order);
	}

	return result;
}

#undef SOLIDIFY_SIDE_NORMALS

static DerivedMesh *applyModifierEM(ModifierData *md,
							 Object *ob,
							 struct EditMesh *editData,
							 DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_Solidify = {
	/* name */              "Solidify",
	/* structName */        "SolidifyModifierData",
	/* structSize */        sizeof(SolidifyModifierData),
	/* type */              eModifierTypeType_Constructive,

	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
