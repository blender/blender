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

#include "BKE_action.h" /* BKE_pose_channel_find_name */
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"

#include "depsgraph_private.h"

#include "MOD_util.h"

static void copyData(ModifierData *md, ModifierData *target)
{
	MaskModifierData *mmd = (MaskModifierData *) md;
	MaskModifierData *tmmd = (MaskModifierData *) target;
	
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

	if (mmd->ob_arm) {
		bArmature *arm = (bArmature *)mmd->ob_arm->data;
		DagNode *armNode = dag_get_node(forest, mmd->ob_arm);
		
		/* tag relationship in depsgraph, but also on the armature */
		dag_add_relation(forest, armNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Mask Modifier");
		arm->flag |= ARM_HAS_VIZ_DEPS;
	}
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	MaskModifierData *mmd = (MaskModifierData *)md;
	DerivedMesh *dm = derivedData, *result = NULL;
	GHash *vertHash = NULL, *edgeHash, *polyHash;
	GHashIterator *hashIter;
	MDeformVert *dvert = NULL, *dv;
	int numPolys = 0, numLoops = 0, numEdges = 0, numVerts = 0;
	int maxVerts, maxEdges, maxPolys;
	int i;

	MPoly *mpoly;
	MLoop *mloop;

	MPoly *mpoly_new;
	MLoop *mloop_new;
	MEdge *medge_new;
	MVert *mvert_new;


	int *loop_mapping;

	/* Overview of Method:
	 *	1. Get the vertices that are in the vertexgroup of interest 
	 *	2. Filter out unwanted geometry (i.e. not in vertexgroup), by populating mappings with new vs old indices
	 *	3. Make a new mesh containing only the mapping data
	 */
	
	/* get original number of verts, edges, and faces */
	maxVerts = dm->getNumVerts(dm);
	maxEdges = dm->getNumEdges(dm);
	maxPolys = dm->getNumPolys(dm);
	
	/* check if we can just return the original mesh 
	 *	- must have verts and therefore verts assigned to vgroups to do anything useful
	 */
	if (!(ELEM(mmd->mode, MOD_MASK_MODE_ARM, MOD_MASK_MODE_VGROUP)) ||
	    (maxVerts == 0) || (ob->defbase.first == NULL) )
	{
		return derivedData;
	}
	
	/* if mode is to use selected armature bones, aggregate the bone groups */
	if (mmd->mode == MOD_MASK_MODE_ARM) { /* --- using selected bones --- */
		Object *oba = mmd->ob_arm;
		bPoseChannel *pchan;
		bDeformGroup *def;
		char *bone_select_array;
		int bone_select_tot = 0;
		const int defbase_tot = BLI_countlist(&ob->defbase);
		
		/* check that there is armature object with bones to use, otherwise return original mesh */
		if (ELEM3(NULL, oba, oba->pose, ob->defbase.first))
			return derivedData;
		
		/* determine whether each vertexgroup is associated with a selected bone or not 
		 * - each cell is a boolean saying whether bone corresponding to the ith group is selected
		 * - groups that don't match a bone are treated as not existing (along with the corresponding ungrouped verts)
		 */
		bone_select_array = MEM_mallocN(defbase_tot * sizeof(char), "mask array");
		
		for (i = 0, def = ob->defbase.first; def; def = def->next, i++) {
			pchan = BKE_pose_channel_find_name(oba->pose, def->name);
			if (pchan && pchan->bone && (pchan->bone->flag & BONE_SELECTED)) {
				bone_select_array[i] = TRUE;
				bone_select_tot++;
			}
			else {
				bone_select_array[i] = FALSE;
			}
		}
		
		/* if no dverts (i.e. no data for vertex groups exists), we've got an
		 * inconsistent situation, so free hashes and return oirginal mesh
		 */
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
		if (dvert == NULL) {
			MEM_freeN(bone_select_array);
			return derivedData;
		}
		
		/* verthash gives mapping from original vertex indices to the new indices (including selected matches only)
		 * 	key=oldindex, value=newindex 
		 */
		vertHash = BLI_ghash_int_new("mask vert gh");
		
		/* add vertices which exist in vertexgroups into vertHash for filtering 
		 * - dv = for each vertex, what vertexgroups does it belong to
		 * - dw = weight that vertex was assigned to a vertexgroup it belongs to
		 */
		for (i = 0, dv = dvert; i < maxVerts; i++, dv++) {
			MDeformWeight *dw = dv->dw;
			short found = 0;
			int j;
			
			/* check the groups that vertex is assigned to, and see if it was any use */
			for (j = 0; j < dv->totweight; j++, dw++) {
				if (dw->def_nr < defbase_tot) {
					if (bone_select_array[dw->def_nr]) {
						if (dw->weight != 0.0f) {
							found = TRUE;
							break;
						}
					}
				}
			}
			
			/* check if include vert in vertHash */
			if (mmd->flag & MOD_MASK_INV) {
				/* if this vert is in the vgroup, don't include it in vertHash */
				if (found) continue;
			}
			else {
				/* if this vert isn't in the vgroup, don't include it in vertHash */
				if (!found) continue;
			}
			
			/* add to ghash for verts (numVerts acts as counter for mapping) */
			BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numVerts));
			numVerts++;
		}
		
		/* free temp hashes */
		MEM_freeN(bone_select_array);
	}
	else {  /* --- Using Nominated VertexGroup only --- */
		int defgrp_index = defgroup_name_index(ob, mmd->vgroup);
		
		/* get dverts */
		if (defgrp_index >= 0)
			dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
			
		/* if no vgroup (i.e. dverts) found, return the initial mesh */
		if ((defgrp_index < 0) || (dvert == NULL))
			return dm;
			
		/* hashes for quickly providing a mapping from old to new - use key=oldindex, value=newindex */
		vertHash = BLI_ghash_int_new("mask vert2 bh");
		
		/* add vertices which exist in vertexgroup into ghash for filtering */
		for (i = 0, dv = dvert; i < maxVerts; i++, dv++) {
			const int weight_set = defvert_find_weight(dv, defgrp_index) != 0.0f;
			
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
	edgeHash = BLI_ghash_int_new("mask ed2 gh");
	polyHash = BLI_ghash_int_new("mask fa2 gh");
	
	mpoly = dm->getPolyArray(dm);
	mloop = dm->getLoopArray(dm);

	loop_mapping = MEM_callocN(sizeof(int) * maxPolys, "mask loopmap"); /* overalloc, assume all polys are seen */

	/* loop over edges and faces, and do the same thing to 
	 * ensure that they only reference existing verts 
	 */
	for (i = 0; i < maxEdges; i++) {
		MEdge me;
		dm->getEdge(dm, i, &me);
		
		/* only add if both verts will be in new mesh */
		if (BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v1)) &&
		    BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v2)))
		{
			BLI_ghash_insert(edgeHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numEdges));
			numEdges++;
		}
	}
	for (i = 0; i < maxPolys; i++) {
		MPoly *mp = &mpoly[i];
		MLoop *ml = mloop + mp->loopstart;
		int ok = TRUE;
		int j;
		
		for (j = 0; j < mp->totloop; j++, ml++) {
			if (!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(ml->v))) {
				ok = FALSE;
				break;
			}
		}
		
		/* all verts must be available */
		if (ok) {
			BLI_ghash_insert(polyHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numPolys));
			loop_mapping[numPolys] = numLoops;
			numPolys++;
			numLoops += mp->totloop;
		}
	}
	
	
	/* now we know the number of verts, edges and faces, 
	 * we can create the new (reduced) mesh
	 */
	result = CDDM_from_template(dm, numVerts, numEdges, 0, numLoops, numPolys);
	
	mpoly_new = CDDM_get_polys(result);
	mloop_new = CDDM_get_loops(result);
	medge_new = CDDM_get_edges(result);
	mvert_new = CDDM_get_verts(result);
	
	/* using ghash-iterators, map data into new mesh */
	/* vertices */
	for (hashIter = BLI_ghashIterator_new(vertHash);
	     !BLI_ghashIterator_isDone(hashIter);
	     BLI_ghashIterator_step(hashIter) )
	{
		MVert source;
		MVert *dest;
		int oldIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(hashIter));
		int newIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getValue(hashIter));
		
		dm->getVert(dm, oldIndex, &source);
		dest = &mvert_new[newIndex];
		
		DM_copy_vert_data(dm, result, oldIndex, newIndex, 1);
		*dest = source;
	}
	BLI_ghashIterator_free(hashIter);
		
	/* edges */
	for (hashIter = BLI_ghashIterator_new(edgeHash);
	     !BLI_ghashIterator_isDone(hashIter);
	     BLI_ghashIterator_step(hashIter))
	{
		MEdge source;
		MEdge *dest;
		int oldIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(hashIter));
		int newIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getValue(hashIter));
		
		dm->getEdge(dm, oldIndex, &source);
		dest = &medge_new[newIndex];
		
		source.v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v1)));
		source.v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v2)));
		
		DM_copy_edge_data(dm, result, oldIndex, newIndex, 1);
		*dest = source;
	}
	BLI_ghashIterator_free(hashIter);
	
	/* faces */
	for (hashIter = BLI_ghashIterator_new(polyHash);
	     !BLI_ghashIterator_isDone(hashIter);
	     BLI_ghashIterator_step(hashIter) )
	{
		int oldIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(hashIter));
		int newIndex = GET_INT_FROM_POINTER(BLI_ghashIterator_getValue(hashIter));
		MPoly *source = &mpoly[oldIndex];
		MPoly *dest = &mpoly_new[newIndex];
		int oldLoopIndex = source->loopstart;
		int newLoopIndex = loop_mapping[newIndex];
		MLoop *source_loop = &mloop[oldLoopIndex];
		MLoop *dest_loop = &mloop_new[newLoopIndex];
		
		DM_copy_poly_data(dm, result, oldIndex, newIndex, 1);
		DM_copy_loop_data(dm, result, oldLoopIndex, newLoopIndex, source->totloop);

		*dest = *source;
		dest->loopstart = newLoopIndex;
		for (i = 0; i < source->totloop; i++) {
			dest_loop[i].v = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source_loop[i].v)));
			dest_loop[i].e = GET_INT_FROM_POINTER(BLI_ghash_lookup(edgeHash, SET_INT_IN_POINTER(source_loop[i].e)));
		}
	}

	BLI_ghashIterator_free(hashIter);

	MEM_freeN(loop_mapping);

	/* why is this needed? - campbell */
	/* recalculate normals */
	CDDM_calc_normals(result);
	
	/* free hashes */
	BLI_ghash_free(vertHash, NULL, NULL);
	BLI_ghash_free(edgeHash, NULL, NULL);
	BLI_ghash_free(polyHash, NULL, NULL);

	/* return the new mesh */
	return result;
}


ModifierTypeInfo modifierType_Mask = {
	/* name */              "Mask",
	/* structName */        "MaskModifierData",
	/* structSize */        sizeof(MaskModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode,

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
