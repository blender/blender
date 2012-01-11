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

/** \file blender/modifiers/intern/MOD_hook.c
 *  \ingroup modifiers
 */


#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_action.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"


#include "depsgraph_private.h"
#include "MEM_guardedalloc.h"

#include "MOD_util.h"

static void initData(ModifierData *md) 
{
	HookModifierData *hmd = (HookModifierData*) md;

	hmd->force= 1.0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	HookModifierData *hmd = (HookModifierData*) md;
	HookModifierData *thmd = (HookModifierData*) target;

	copy_v3_v3(thmd->cent, hmd->cent);
	thmd->falloff = hmd->falloff;
	thmd->force = hmd->force;
	thmd->object = hmd->object;
	thmd->totindex = hmd->totindex;
	thmd->indexar = MEM_dupallocN(hmd->indexar);
	memcpy(thmd->parentinv, hmd->parentinv, sizeof(hmd->parentinv));
	BLI_strncpy(thmd->name, hmd->name, sizeof(thmd->name));
	BLI_strncpy(thmd->subtarget, hmd->subtarget, sizeof(thmd->subtarget));
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(hmd->name[0]) dataMask |= CD_MASK_MDEFORMVERT;
	if(hmd->indexar) dataMask |= CD_MASK_ORIGINDEX;

	return dataMask;
}

static void freeData(ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData*) md;

	if (hmd->indexar) MEM_freeN(hmd->indexar);
}

static int isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	HookModifierData *hmd = (HookModifierData*) md;

	return !hmd->object;
}

static void foreachObjectLink(
					   ModifierData *md, Object *ob,
	void (*walk)(void *userData, Object *ob, Object **obpoin),
		   void *userData)
{
	HookModifierData *hmd = (HookModifierData*) md;

	walk(userData, ob, &hmd->object);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
						struct Scene *UNUSED(scene),
						Object *UNUSED(ob),
						DagNode *obNode)
{
	HookModifierData *hmd = (HookModifierData*) md;

	if (hmd->object) {
		DagNode *curNode = dag_get_node(forest, hmd->object);
		
		if (hmd->subtarget[0])
			dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA|DAG_RL_DATA_DATA, "Hook Modifier");
		else
			dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA, "Hook Modifier");
	}
}

static float hook_falloff(float *co_1, float *co_2, const float falloff_squared, float fac)
{
	if(falloff_squared) {
		float len_squared = len_squared_v3v3(co_1, co_2);
		if(len_squared > falloff_squared) {
			return 0.0f;
		}
		else if(len_squared > 0.0f) {
			return fac * (1.0f - (len_squared / falloff_squared));
		}
	}

	return fac;
}

static void deformVerts_do(HookModifierData *hmd, Object *ob, DerivedMesh *dm,
                           float (*vertexCos)[3], int numVerts)
{
	bPoseChannel *pchan= get_pose_channel(hmd->object->pose, hmd->subtarget);
	float vec[3], mat[4][4], dmat[4][4];
	int i, *index_pt;
	const float falloff_squared= hmd->falloff * hmd->falloff; /* for faster comparisons */
	
	MDeformVert *dvert;
	int defgrp_index, max_dvert;
	
	/* get world-space matrix of target, corrected for the space the verts are in */
	if (hmd->subtarget[0] && pchan) {
		/* bone target if there's a matching pose-channel */
		mult_m4_m4m4(dmat, hmd->object->obmat, pchan->pose_mat);
	}
	else {
		/* just object target */
		copy_m4_m4(dmat, hmd->object->obmat);
	}
	invert_m4_m4(ob->imat, ob->obmat);
	mul_serie_m4(mat, ob->imat, dmat, hmd->parentinv,
	             NULL, NULL, NULL, NULL, NULL);

	modifier_get_vgroup(ob, dm, hmd->name, &dvert, &defgrp_index);
	max_dvert = (dvert)? numVerts: 0;

	/* Regarding index range checking below.
	 *
	 * This should always be true and I don't generally like 
	 * "paranoid" style code like this, but old files can have
	 * indices that are out of range because old blender did
	 * not correct them on exit editmode. - zr
	 */
	
	if(hmd->force == 0.0f) {
		/* do nothing, avoid annoying checks in the loop */
	}
	else if(hmd->indexar) { /* vertex indices? */
		const float fac_orig= hmd->force;
		float fac;
		const int *origindex_ar;
		
		/* if DerivedMesh is present and has original index data, use it */
		if(dm && (origindex_ar= dm->getVertDataArray(dm, CD_ORIGINDEX))) {
			for(i= 0, index_pt= hmd->indexar; i < hmd->totindex; i++, index_pt++) {
				if(*index_pt < numVerts) {
					int j;
					
					for(j = 0; j < numVerts; j++) {
						if(origindex_ar[j] == *index_pt) {
							float *co = vertexCos[j];
							if((fac= hook_falloff(hmd->cent, co, falloff_squared, fac_orig))) {
								if(dvert)
									fac *= defvert_find_weight(dvert+j, defgrp_index);
								
								if(fac) {
									mul_v3_m4v3(vec, mat, co);
									interp_v3_v3v3(co, co, vec, fac);
								}
							}
						}
					}
				}
			}
		}
		else { /* missing dm or ORIGINDEX */
			for(i= 0, index_pt= hmd->indexar; i < hmd->totindex; i++, index_pt++) {
				if(*index_pt < numVerts) {
					float *co = vertexCos[*index_pt];
					if((fac= hook_falloff(hmd->cent, co, falloff_squared, fac_orig))) {
						if(dvert)
							fac *= defvert_find_weight(dvert+(*index_pt), defgrp_index);
						
						if(fac) {
							mul_v3_m4v3(vec, mat, co);
							interp_v3_v3v3(co, co, vec, fac);
						}
					}
				}
			}
		}
	}
	else if(dvert) {	/* vertex group hook */
		const float fac_orig= hmd->force;
		
		for(i = 0; i < max_dvert; i++, dvert++) {
			float fac;
			float *co = vertexCos[i];
			
			if((fac= hook_falloff(hmd->cent, co, falloff_squared, fac_orig))) {
				fac *= defvert_find_weight(dvert, defgrp_index);
				if(fac) {
					mul_v3_m4v3(vec, mat, co);
					interp_v3_v3v3(co, co, vec, fac);
				}
			}
		}
	}
}

static void deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                        float (*vertexCos)[3], int numVerts,
                        int UNUSED(useRenderParams), int UNUSED(isFinalCalc))
{
	HookModifierData *hmd = (HookModifierData*) md;
	DerivedMesh *dm = derivedData;
	/* We need a valid dm for meshes when a vgroup is set... */
	if(!dm && ob->type == OB_MESH && hmd->name[0] != '\0')
		dm = get_dm(ob, NULL, dm, NULL, 0);

	deformVerts_do(hmd, ob, dm, vertexCos, numVerts);

	if(derivedData != dm)
		dm->release(dm);
}

static void deformVertsEM(ModifierData *md, Object *ob, struct EditMesh *editData,
                          DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	HookModifierData *hmd = (HookModifierData*) md;
	DerivedMesh *dm = derivedData;
	/* We need a valid dm for meshes when a vgroup is set... */
	if(!dm && ob->type == OB_MESH && hmd->name[0] != '\0')
		dm = get_dm(ob, editData, dm, NULL, 0);

	deformVerts_do(hmd, ob, dm, vertexCos, numVerts);

	if(derivedData != dm)
		dm->release(dm);
}


ModifierTypeInfo modifierType_Hook = {
	/* name */              "Hook",
	/* structName */        "HookModifierData",
	/* structSize */        sizeof(HookModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsEditmode,
	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
