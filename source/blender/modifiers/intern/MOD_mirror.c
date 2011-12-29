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

/** \file blender/modifiers/intern/MOD_mirror.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"


#include "MEM_guardedalloc.h"
#include "depsgraph_private.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	mmd->flag |= (MOD_MIR_AXIS_X | MOD_MIR_VGROUP);
	mmd->tolerance = 0.001;
	mmd->mirror_ob = NULL;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;
	MirrorModifierData *tmmd = (MirrorModifierData*) target;

	tmmd->flag = mmd->flag;
	tmmd->tolerance = mmd->tolerance;
	tmmd->mirror_ob = mmd->mirror_ob;
}

static void foreachObjectLink(
						 ModifierData *md, Object *ob,
	  void (*walk)(void *userData, Object *ob, Object **obpoin),
		 void *userData)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	walk(userData, ob, &mmd->mirror_ob);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
						struct Scene *UNUSED(scene),
						Object *UNUSED(ob),
						DagNode *obNode)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	if(mmd->mirror_ob) {
		DagNode *latNode = dag_get_node(forest, mmd->mirror_ob);

		dag_add_relation(forest, latNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Mirror Modifier");
	}
}

static DerivedMesh *doMirrorOnAxis(MirrorModifierData *mmd,
                                   Object *ob,
                                   DerivedMesh *dm,
                                   int axis)
{
	int i;
	float tolerance = mmd->tolerance;
	DerivedMesh *result;
	int numVerts, numEdges, numFaces;
	int maxVerts = dm->getNumVerts(dm);
	int maxEdges = dm->getNumEdges(dm);
	int maxFaces = dm->getNumFaces(dm);
	int *flip_map= NULL, flip_map_len= 0;
	int do_vgroup_mirr= (mmd->flag & MOD_MIR_VGROUP);
	unsigned int (*indexMap)[2];
	float mtx[4][4], imtx[4][4];

	numVerts = numEdges = numFaces = 0;

	indexMap = MEM_mallocN(sizeof(*indexMap) * maxVerts, "indexmap");

	result = CDDM_from_template(dm, maxVerts * 2, maxEdges * 2, maxFaces * 2);


	if (do_vgroup_mirr) {
		flip_map= defgroup_flip_map(ob, &flip_map_len, FALSE);
		if(flip_map == NULL)
			do_vgroup_mirr= 0;
	}

	if (mmd->mirror_ob) {
		float obinv[4][4];
		
		invert_m4_m4(obinv, mmd->mirror_ob->obmat);
		mult_m4_m4m4(mtx, obinv, ob->obmat);
		invert_m4_m4(imtx, mtx);
	}

	for(i = 0; i < maxVerts; i++) {
		MVert inMV;
		MVert *mv = CDDM_get_vert(result, numVerts);
		int isShared;
		float co[3];
		
		dm->getVert(dm, i, &inMV);
		
		copy_v3_v3(co, inMV.co);
		
		if (mmd->mirror_ob) {
			mul_m4_v3(mtx, co);
		}
		
		if(mmd->flag & MOD_MIR_NO_MERGE)
			isShared = 0;
		else
			isShared = ABS(co[axis])<=tolerance;
		
		/* Because the topology result (# of vertices) must be the same if
		 * the mesh data is overridden by vertex cos, have to calc sharedness
		 * based on original coordinates. This is why we test before copy.
		 */
		DM_copy_vert_data(dm, result, i, numVerts, 1);
		*mv = inMV;

		indexMap[i][0] = numVerts;
		indexMap[i][1] = !isShared;

		numVerts++;

		if(isShared ) {
			co[axis] = 0.0f;
			if (mmd->mirror_ob) {
				mul_m4_v3(imtx, co);
			}
			copy_v3_v3(mv->co, co);
			
			mv->flag |= ME_VERT_MERGED;
		}
		else {
			MVert *mv2 = CDDM_get_vert(result, numVerts);
			
			DM_copy_vert_data(dm, result, i, numVerts, 1);
			*mv2 = *mv;
			
			co[axis] = -co[axis];
			if (mmd->mirror_ob) {
				mul_m4_v3(imtx, co);
			}
			copy_v3_v3(mv2->co, co);
			
			if (do_vgroup_mirr) {
				MDeformVert *dvert= DM_get_vert_data(result, numVerts, CD_MDEFORMVERT);
				if(dvert) {
					defvert_flip(dvert, flip_map, flip_map_len);
				}
			}

			numVerts++;
		}
	}

	for(i = 0; i < maxEdges; i++) {
		MEdge inMED;
		MEdge *med = CDDM_get_edge(result, numEdges);
		
		dm->getEdge(dm, i, &inMED);
		
		DM_copy_edge_data(dm, result, i, numEdges, 1);
		*med = inMED;
		numEdges++;
		
		med->v1 = indexMap[inMED.v1][0];
		med->v2 = indexMap[inMED.v2][0];
		
		if(indexMap[inMED.v1][1] || indexMap[inMED.v2][1]) {
			MEdge *med2 = CDDM_get_edge(result, numEdges);
			
			DM_copy_edge_data(dm, result, i, numEdges, 1);
			*med2 = *med;
			numEdges++;
			
			med2->v1 += indexMap[inMED.v1][1];
			med2->v2 += indexMap[inMED.v2][1];
		}
	}

	for(i = 0; i < maxFaces; i++) {
		MFace inMF;
		MFace *mf = CDDM_get_face(result, numFaces);
		
		dm->getFace(dm, i, &inMF);
		
		DM_copy_face_data(dm, result, i, numFaces, 1);
		*mf = inMF;
		numFaces++;
		
		mf->v1 = indexMap[inMF.v1][0];
		mf->v2 = indexMap[inMF.v2][0];
		mf->v3 = indexMap[inMF.v3][0];
		mf->v4 = indexMap[inMF.v4][0];
		
		if ( indexMap[inMF.v1][1] ||
		     indexMap[inMF.v2][1] ||
		     indexMap[inMF.v3][1] ||
		     (mf->v4 && indexMap[inMF.v4][1]))
		{
			MFace *mf2 = CDDM_get_face(result, numFaces);
			static int corner_indices[4] = {2, 1, 0, 3};
			
			DM_copy_face_data(dm, result, i, numFaces, 1);
			*mf2 = *mf;
			
			mf2->v1 += indexMap[inMF.v1][1];
			mf2->v2 += indexMap[inMF.v2][1];
			mf2->v3 += indexMap[inMF.v3][1];
			if(inMF.v4) mf2->v4 += indexMap[inMF.v4][1];
			
			/* mirror UVs if enabled */
			if(mmd->flag & (MOD_MIR_MIRROR_U | MOD_MIR_MIRROR_V)) {
				MTFace *tf = result->getFaceData(result, numFaces, CD_MTFACE);
				if(tf) {
					int j;
					for(j = 0; j < 4; ++j) {
						if(mmd->flag & MOD_MIR_MIRROR_U)
							tf->uv[j][0] = 1.0f - tf->uv[j][0];
						if(mmd->flag & MOD_MIR_MIRROR_V)
							tf->uv[j][1] = 1.0f - tf->uv[j][1];
					}
				}
			}
			
			/* Flip face normal */
			SWAP(unsigned int, mf2->v1, mf2->v3);
			DM_swap_face_data(result, numFaces, corner_indices);
			
			test_index_face(mf2, &result->faceData, numFaces, inMF.v4?4:3);
			numFaces++;
		}
	}

	if (flip_map) MEM_freeN(flip_map);

	MEM_freeN(indexMap);

	CDDM_lower_num_verts(result, numVerts);
	CDDM_lower_num_edges(result, numEdges);
	CDDM_lower_num_faces(result, numFaces);

	return result;
}

static DerivedMesh *mirrorModifier__doMirror(MirrorModifierData *mmd,
						Object *ob, DerivedMesh *dm)
{
	DerivedMesh *result = dm;

	/* check which axes have been toggled and mirror accordingly */
	if(mmd->flag & MOD_MIR_AXIS_X) {
		result = doMirrorOnAxis(mmd, ob, result, 0);
	}
	if(mmd->flag & MOD_MIR_AXIS_Y) {
		DerivedMesh *tmp = result;
		result = doMirrorOnAxis(mmd, ob, result, 1);
		if(tmp != dm) tmp->release(tmp); /* free intermediate results */
	}
	if(mmd->flag & MOD_MIR_AXIS_Z) {
		DerivedMesh *tmp = result;
		result = doMirrorOnAxis(mmd, ob, result, 2);
		if(tmp != dm) tmp->release(tmp); /* free intermediate results */
	}

	return result;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	DerivedMesh *result;
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	result = mirrorModifier__doMirror(mmd, ob, derivedData);

	if(result != derivedData)
		CDDM_calc_normals(result);
	
	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
						struct EditMesh *UNUSED(editData),
						DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_Mirror = {
	/* name */              "Mirror",
	/* structName */        "MirrorModifierData",
	/* structSize */        sizeof(MirrorModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsMapping
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode
							| eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
