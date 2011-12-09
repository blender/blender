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


#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_ghash.h"

#include "DNA_armature_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h" /* get_pose_channel */
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"

#include "depsgraph_private.h"

#include "MOD_util.h"

static void copyData(ModifierData *md, ModifierData *target)
{
	MaskModifierData *mmd = (MaskModifierData*) md;
	MaskModifierData *tmmd = (MaskModifierData*) target;
	
	BLI_strncpy(tmmd->vgroup, mmd->vgroup, sizeof(tmmd->vgroup));
	tmmd->flag = mmd->flag;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *UNUSED(md))
{
	return CD_MASK_MDEFORMVERT;
}

static void foreachObjectLink(
						  ModifierData *md, Object *ob,
	   void (*walk)(void *userData, Object *ob, Object **obpoin),
		  void *userData)
{
	MaskModifierData *mmd = (MaskModifierData *)md;
	walk(userData, ob, &mmd->ob_arm);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
						struct Scene *UNUSED(scene),
						Object *UNUSED(ob),
						DagNode *obNode)
{
	MaskModifierData *mmd = (MaskModifierData *)md;

	if (mmd->ob_arm) 
	{
		DagNode *armNode = dag_get_node(forest, mmd->ob_arm);
		
		dag_add_relation(forest, armNode, obNode,
				DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Mask Modifier");
	}
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	MaskModifierData *mmd= (MaskModifierData *)md;
	DerivedMesh *dm= derivedData, *result= NULL;
	GHash *vertHash=NULL, *edgeHash, *faceHash;
	GHashIterator *hashIter;
	MDeformVert *dvert= NULL, *dv;
	int numFaces=0, numEdges=0, numVerts=0;
	int maxVerts, maxEdges, maxFaces;
	int i;
	
	/* Overview of Method:
	 *	1. Get the vertices that are in the vertexgroup of interest 
	 *	2. Filter out unwanted geometry (i.e. not in vertexgroup), by populating mappings with new vs old indices
	 *	3. Make a new mesh containing only the mapping data
	 */
	
	/* get original number of verts, edges, and faces */
	maxVerts= dm->getNumVerts(dm);
	maxEdges= dm->getNumEdges(dm);
	maxFaces= dm->getNumFaces(dm);
	
	/* check if we can just return the original mesh 
	 *	- must have verts and therefore verts assigned to vgroups to do anything useful
	 */
	if ( !(ELEM(mmd->mode, MOD_MASK_MODE_ARM, MOD_MASK_MODE_VGROUP)) ||
		 (maxVerts == 0) || (ob->defbase.first == NULL) )
	{
		return derivedData;
	}
	
	/* if mode is to use selected armature bones, aggregate the bone groups */
	if (mmd->mode == MOD_MASK_MODE_ARM) /* --- using selected bones --- */
	{
		GHash *vgroupHash;
		Object *oba= mmd->ob_arm;
		bPoseChannel *pchan;
		bDeformGroup *def;
		char *bone_select_array;
		int bone_select_tot= 0;
		const int defbase_tot= BLI_countlist(&ob->defbase);
		
		/* check that there is armature object with bones to use, otherwise return original mesh */
		if (ELEM3(NULL, mmd->ob_arm, mmd->ob_arm->pose, ob->defbase.first))
			return derivedData;

		bone_select_array= MEM_mallocN(defbase_tot * sizeof(char), "mask array");

		for (i = 0, def = ob->defbase.first; def; def = def->next, i++)
		{
			if (((pchan= get_pose_channel(oba->pose, def->name)) && pchan->bone && (pchan->bone->flag & BONE_SELECTED)))
			{
				bone_select_array[i]= TRUE;
				bone_select_tot++;
			}
			else {
				bone_select_array[i]= FALSE;
			}
		}

		/* hashes for finding mapping of:
		 * 	- vgroups to indices -> vgroupHash  (string, int)
		 *	- bones to vgroup indices -> boneHash (index of vgroup, dummy)
		 */
		vgroupHash= BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "mask vgroup gh");
		
		/* build mapping of names of vertex groups to indices */
		for (i = 0, def = ob->defbase.first; def; def = def->next, i++) 
			BLI_ghash_insert(vgroupHash, def->name, SET_INT_IN_POINTER(i));
		
		/* if no bones selected, free hashes and return original mesh */
		if (bone_select_tot == 0)
		{
			BLI_ghash_free(vgroupHash, NULL, NULL);
			MEM_freeN(bone_select_array);
			
			return derivedData;
		}
		
		/* repeat the previous check, but for dverts */
		dvert= dm->getVertDataArray(dm, CD_MDEFORMVERT);
		if (dvert == NULL)
		{
			BLI_ghash_free(vgroupHash, NULL, NULL);
			MEM_freeN(bone_select_array);
			
			return derivedData;
		}
		
		/* hashes for quickly providing a mapping from old to new - use key=oldindex, value=newindex */
		vertHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp, "mask vert gh");
		
		/* add vertices which exist in vertexgroups into vertHash for filtering */
		for (i= 0, dv= dvert; i < maxVerts; i++, dv++)
		{
			MDeformWeight *dw= dv->dw;
			int j;

			for (j= dv->totweight; j > 0; j--, dw++) {
				if (dw->def_nr < defbase_tot) {
					if (bone_select_array[dw->def_nr]) {
						if(dw->weight != 0.0f) {
							break;
						}
					}
				}
			}
			
			/* check if include vert in vertHash */
			if (mmd->flag & MOD_MASK_INV) {
				/* if this vert is in the vgroup, don't include it in vertHash */
				if (dw) continue;
			}
			else {
				/* if this vert isn't in the vgroup, don't include it in vertHash */
				if (!dw) continue;
			}
			
			/* add to ghash for verts (numVerts acts as counter for mapping) */
			BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numVerts));
			numVerts++;
		}
		
		/* free temp hashes */
		BLI_ghash_free(vgroupHash, NULL, NULL);
		MEM_freeN(bone_select_array);
	}
	else		/* --- Using Nominated VertexGroup only --- */ 
	{
		int defgrp_index = defgroup_name_index(ob, mmd->vgroup);
		
		/* get dverts */
		if (defgrp_index >= 0)
			dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
			
		/* if no vgroup (i.e. dverts) found, return the initial mesh */
		if ((defgrp_index < 0) || (dvert == NULL))
			return dm;
			
		/* hashes for quickly providing a mapping from old to new - use key=oldindex, value=newindex */
		vertHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp, "mask vert2 bh");
		
		/* add vertices which exist in vertexgroup into ghash for filtering */
		for (i= 0, dv= dvert; i < maxVerts; i++, dv++)
		{
			const int weight_set= defvert_find_weight(dv, defgrp_index) != 0.0f;
			
			/* check if include vert in vertHash */
			if (mmd->flag & MOD_MASK_INV) {
				/* if this vert is in the vgroup, don't include it in vertHash */
				if (weight_set) continue;
			}
			else {
				/* if this vert isn't in the vgroup, don't include it in vertHash */
				if (!weight_set) continue;
			}
			
			/* add to ghash for verts (numVerts acts as counter for mapping) */
			BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numVerts));
			numVerts++;
		}
	}
	
	/* hashes for quickly providing a mapping from old to new - use key=oldindex, value=newindex */
	edgeHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp, "mask ed2 gh");
	faceHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp, "mask fa2 gh");
	
	/* loop over edges and faces, and do the same thing to 
	 * ensure that they only reference existing verts 
	 */
	for (i = 0; i < maxEdges; i++) 
	{
		MEdge me;
		dm->getEdge(dm, i, &me);
		
		/* only add if both verts will be in new mesh */
		if ( BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v1)) &&
			 BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v2)) )
		{
			BLI_ghash_insert(edgeHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numEdges));
			numEdges++;
		}
	}
	for (i = 0; i < maxFaces; i++) 
	{
		MFace mf;
		dm->getFace(dm, i, &mf);
		
		/* all verts must be available */
		if ( BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v1)) &&
			 BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v2)) &&
			 BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v3)) &&
			(mf.v4==0 || BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v4))) )
		{
			BLI_ghash_insert(faceHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numFaces));
			numFaces++;
		}
	}
	
	
	/* now we know the number of verts, edges and faces, 
	 * we can create the new (reduced) mesh
	 */
	result = CDDM_from_template(dm, numVerts, numEdges, numFaces);
	
	
	/* using ghash-iterators, map data into new mesh */
		/* vertices */
	for ( hashIter = BLI_ghashIterator_new(vertHash);
		  !BLI_ghashIterator_isDone(hashIter);
		  BLI_ghashIterator_step(hashIter) ) 
	{
		MVert source;
		MVert *dest;
		int oldIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(hashIter));
		int newIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getValue(hashIter));
		
		dm->getVert(dm, oldIndex, &source);
		dest = CDDM_get_vert(result, newIndex);
		
		DM_copy_vert_data(dm, result, oldIndex, newIndex, 1);
		*dest = source;
	}
	BLI_ghashIterator_free(hashIter);
		
		/* edges */
	for ( hashIter = BLI_ghashIterator_new(edgeHash);
		  !BLI_ghashIterator_isDone(hashIter);
		  BLI_ghashIterator_step(hashIter) ) 
	{
		MEdge source;
		MEdge *dest;
		int oldIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(hashIter));
		int newIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getValue(hashIter));
		
		dm->getEdge(dm, oldIndex, &source);
		dest = CDDM_get_edge(result, newIndex);
		
		source.v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v1)));
		source.v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v2)));
		
		DM_copy_edge_data(dm, result, oldIndex, newIndex, 1);
		*dest = source;
	}
	BLI_ghashIterator_free(hashIter);
	
		/* faces */
	for ( hashIter = BLI_ghashIterator_new(faceHash);
		  !BLI_ghashIterator_isDone(hashIter);
		  BLI_ghashIterator_step(hashIter) ) 
	{
		MFace source;
		MFace *dest;
		int oldIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(hashIter));
		int newIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getValue(hashIter));
		int orig_v4;
		
		dm->getFace(dm, oldIndex, &source);
		dest = CDDM_get_face(result, newIndex);
		
		orig_v4 = source.v4;
		
		source.v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v1)));
		source.v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v2)));
		source.v3 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v3)));
		if (source.v4)
		   source.v4 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v4)));
		
		DM_copy_face_data(dm, result, oldIndex, newIndex, 1);
		*dest = source;
		
		test_index_face(dest, &result->faceData, newIndex, (orig_v4 ? 4 : 3));
	}
	BLI_ghashIterator_free(hashIter);
	
	/* recalculate normals */
	CDDM_calc_normals(result);
	
	/* free hashes */
	BLI_ghash_free(vertHash, NULL, NULL);
	BLI_ghash_free(edgeHash, NULL, NULL);
	BLI_ghash_free(faceHash, NULL, NULL);
	
	/* return the new mesh */
	return result;
}


ModifierTypeInfo modifierType_Mask = {
	/* name */              "Mask",
	/* structName */        "MaskModifierData",
	/* structSize */        sizeof(MaskModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh|eModifierTypeFlag_SupportsMapping|eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          NULL,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
