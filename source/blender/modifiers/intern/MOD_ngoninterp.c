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

/** \file blender/modifiers/intern/MOD_mask.c
 *  \ingroup modifiers
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_array.h"
#include "BLI_edgehash.h"
#include "BLI_math.h"

#include "DNA_armature_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"

#include "depsgraph_private.h"

#include "MOD_util.h"

static void copyData(ModifierData *md, ModifierData *target)
{
	NgonInterpModifierData *mmd = (NgonInterpModifierData*) md;
	NgonInterpModifierData *tmmd = (NgonInterpModifierData*) target;
	
	tmmd->resolution = mmd->resolution;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *UNUSED(ob),
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	NgonInterpModifierData *nmd= (NgonInterpModifierData *)md;
	DerivedMesh *dm= derivedData;
	DerivedMesh *cddm, *dummy;
	MFace *mf;
	MPoly *mpoly;
	MLoop *mloop;
	MFace *mface = NULL, *mf2;
	MVert *mvert = NULL, *omvert;
	BLI_array_declare(mface);
	BLI_array_declare(mvert);
	int *verts=NULL, *loops=NULL;
	BLI_array_declare(verts);
	BLI_array_declare(loops);
	float *w = NULL, (*cos)[3] = NULL;
	BLI_array_declare(w);
	BLI_array_declare(cos);
	int *origv = NULL, *origf = NULL, *of, *ov;
	BLI_array_declare(origv);
	BLI_array_declare(origf);
	DerivedMesh *copy = NULL;
	int i;

	int numTex;
	int numCol;
	int hasWCol;
	int hasOrigSpace;

	if (nmd->resolution <= 0)
		return dm;
	
	if (!CDDM_Check(dm)) {
		dm = copy = CDDM_copy(dm);
	}
	
	CDDM_recalc_tesselation(dm);
	
	mf = dm->getTessFaceArray(dm);
	of = dm->getTessFaceDataArray(dm, CD_ORIGINDEX);
	mpoly = CDDM_get_polys(dm);
	mloop = CDDM_get_loops(dm);
	
	/*eek!*/
	if (!of)
		return dm;

	/*create a dummy mesh to compute interpolated loops on*/
	dummy = CDDM_from_template(dm, 0, 0, 0, 3, 0);

	/* CustomData we check must match what is passed to mesh_loops_to_mface_corners() */
	numTex = CustomData_number_of_layers(&dm->polyData, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&dummy->loopData, CD_MLOOPCOL);
	hasWCol = CustomData_has_layer(&dummy->loopData, CD_WEIGHT_MLOOPCOL);
	hasOrigSpace = CustomData_has_layer(&dummy->loopData, CD_ORIGSPACE_MLOOP);

	/*copy original verts here, so indices stay correct*/
	omvert = dm->getVertArray(dm);
	ov = dm->getVertDataArray(dm, CD_ORIGINDEX);
	for (i=0; i<dm->numVertData; i++) {
		BLI_array_append(mvert, omvert[i]);
		BLI_array_append(origv, ov ? ov[i] : i);
	}
	
	for (i=0; i<dm->numTessFaceData; i++, mf++, of++) {
		int x, y, x2;
		float fac;
		
		BLI_array_empty(verts);
		
#define NG_MAKE_VERT(orig)\
		BLI_array_append(mvert, omvert[orig]);\
		BLI_array_append(origv, ov ? ov[orig] : orig);\
		BLI_array_append(verts, BLI_array_count(mvert)-1);

#define NG_MAKE_VERTCO(orig, coord) NG_MAKE_VERT(orig); copy_v3_v3(mvert[BLI_array_count(mvert)-1].co, coord)

		y = 0;
		fac = 1.0f / (float)(nmd->resolution + 1);
		for (x=0; x<nmd->resolution+2; x++) {
			float co1[3], co2[3], co3[3];
			
			sub_v3_v3v3(co1, omvert[mf->v1].co, omvert[mf->v3].co);
			sub_v3_v3v3(co2, omvert[mf->v2].co, omvert[mf->v3].co);
			
			mul_v3_fl(co1, 1.0f - fac*x);
			mul_v3_fl(co2, 1.0f - fac*x);
			
			add_v3_v3(co1, omvert[mf->v3].co);
			add_v3_v3(co2, omvert[mf->v3].co);

			if (x == 0) {
				BLI_array_append(verts, mf->v1);
			} else if (x == nmd->resolution+1) {
				BLI_array_append(verts, mf->v3);
			} else {
				NG_MAKE_VERTCO(mf->v1, co1);
			}
			
			for (x2=0; x2<(nmd->resolution-x); x2++) {
				sub_v3_v3v3(co3, co1, co2);
				mul_v3_fl(co3, 1.0f - (1.0f/(float)(nmd->resolution-x+1))*(x2+1));
				add_v3_v3(co3, co2);
				
				NG_MAKE_VERTCO(mf->v2, co3);
			}
			
			if (x == 0) {
				BLI_array_append(verts, mf->v2);
			} else if (x != nmd->resolution+1) {
				NG_MAKE_VERTCO(mf->v1, co2);
			}
		}
		
		y = 0;
		for (x=0; x<BLI_array_count(verts)-2; x++) {
			int v1, v2, v3;
			
			if (x2 == nmd->resolution-y+1) {
				x2 = 0;
				y++;
				continue;
			} else {
				/*int lindex[3] = {0, 1, 2};*/ /*UNUSED*/
				
				v1 = verts[x];
				v2 = verts[x+1];
				v3 = verts[x+(nmd->resolution-y)+2];
				
				BLI_array_growone(mface);
				BLI_array_growone(origf);
				
				/*make first face*/
				origf[BLI_array_count(origf)-1] = *of;
				mf2 = mface + BLI_array_count(mface)-1;
				*mf2 = *mf;
				
				mf2->v1 = v1;
				mf2->v2 = v2;
				mf2->v3 = v3;
				mf2->v4 = 0;
				
				if (x2 != nmd->resolution-y) {
					/*make second face*/
					BLI_array_growone(mface);
					BLI_array_growone(origf);
					
					origf[BLI_array_count(origf)-1] = *of;
					mf2 = mface + BLI_array_count(mface)-1;
					*mf2 = *mf;
					
					mf2->v1 = verts[x+(nmd->resolution-y)+3];
					mf2->v2 = v3;
					mf2->v3 = v2;
					mf2->v4 = 0;
				}
			}
			
			x2++;
		}
	}
		
	cddm = CDDM_from_template(dm, BLI_array_count(mvert), dm->numEdgeData, BLI_array_count(mface), 0, 0);
	
	mf2 = mface;
	for (i=0; i<BLI_array_count(mface); i++, mf2++) {
		MPoly *mp = mpoly + *of;
		MLoop *ml;
		float co[3], cent[3] = {0.0f, 0.0f, 0.0f};
		int j, lindex[4] = {0, 1, 2}; /* only ever use 3 in this case */
		
		BLI_array_empty(w);
		BLI_array_empty(cos);
		BLI_array_empty(loops);
		
		mp = mpoly + origf[i];
		ml = mloop + mp->loopstart;
		for (j=0; j<mp->totloop; j++, ml++) {
			BLI_array_growone(cos);
			BLI_array_growone(w);
			BLI_array_append(loops, j+mp->loopstart);
			copy_v3_v3(cos[j], mvert[ml->v].co);
		}
		
		/*scale source face coordinates a bit, so points sitting directly on an
		  edge will work.*/
		mul_v3_fl(cent, 1.0f/(float)(mp->totloop));
		for (j=0; j<mp->totloop; j++) {
			sub_v3_v3(cos[j], cent);
			mul_v3_fl(cos[j], 1.0f+FLT_EPSILON*1500.0f);
			add_v3_v3(cos[j], cent);
		}

		copy_v3_v3(co, (mvert + mf2->v1)->co);
		interp_weights_poly_v3(w, cos, mp->totloop, co);
		CustomData_interp(&dm->loopData, &dummy->loopData, loops, w, NULL, mp->totloop, 0);
		
		copy_v3_v3(co, (mvert + mf2->v2)->co);
		interp_weights_poly_v3(w, cos, mp->totloop, co);
		CustomData_interp(&dm->loopData, &dummy->loopData, loops, w, NULL, mp->totloop, 1);
	
		copy_v3_v3(co, (mvert + mf2->v3)->co);
		interp_weights_poly_v3(w, cos, mp->totloop, co);
		CustomData_interp(&dm->loopData, &dummy->loopData, loops, w, NULL, mp->totloop, 2);
		
		mesh_loops_to_mface_corners(&cddm->faceData, &dummy->loopData, &dm->polyData,
		                            lindex, i, origf[i], 3,
		                            numTex, numCol, hasWCol, hasOrigSpace);
	}
	
	CustomData_copy_data(&dm->vertData, &cddm->vertData, 0, 0, dm->numVertData);
	CustomData_copy_data(&dm->edgeData, &cddm->edgeData, 0, 0, dm->numEdgeData);
	
	CDDM_set_mface(cddm, mface);
	CDDM_set_mvert(cddm, mvert);
	
	/*set origindex pointer*/
	MEM_freeN(CustomData_get_layer(&cddm->faceData, CD_ORIGINDEX));
	CustomData_set_layer(&cddm->faceData, CD_MFACE, mface);
	
	if (CustomData_has_layer(&cddm->vertData, CD_ORIGINDEX))
		CustomData_set_layer(&cddm->vertData, CD_ORIGINDEX, origv);

	CustomData_set_layer(&cddm->faceData, CD_ORIGINDEX, origf);

	BLI_array_free(cos);
	BLI_array_free(w);
	
	dummy->needsFree = 1;
	dummy->release(dummy);
	
	/*create polys from mface triangles*/
	CDDM_tessfaces_to_faces(cddm); /*builds ngon faces from tess (mface) faces*/

	return cddm;
}


ModifierTypeInfo modifierType_NgonInterp = {
	/* name */              "NgonInterp",
	/* structName */        "NgonInterpModifierData",
	/* structSize */        sizeof(NgonInterpModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh|eModifierTypeFlag_SupportsMapping|eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformMatrices */    0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   0,
	/* initData */          0,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     0,
	/* dependsOnNormals */	0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
