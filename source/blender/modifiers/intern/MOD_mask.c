/*
* $Id:
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

#include "stddef.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"

#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_uvproject.h"

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_fluidsim.h"


#include "BKE_action.h"
#include "BKE_bmesh.h"
#include "BKE_booleanops.h"
#include "BKE_cloth.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "LOD_decimation.h"

#include "CCGSubSurf.h"

#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"


static void copyData(ModifierData *md, ModifierData *target)
{
	MaskModifierData *mmd = (MaskModifierData*) md;
	MaskModifierData *tmmd = (MaskModifierData*) target;
	
	strcpy(tmmd->vgroup, mmd->vgroup);
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	return (1 << CD_MDEFORMVERT);
}

static void foreachObjectLink(
						  ModifierData *md, Object *ob,
	   void (*walk)(void *userData, Object *ob, Object **obpoin),
		  void *userData)
{
	MaskModifierData *mmd = (MaskModifierData *)md;
	walk(userData, ob, &mmd->ob_arm);
}

static void updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene,
					   Object *ob, DagNode *obNode)
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
  int useRenderParams, int isFinalCalc)
{
	MaskModifierData *mmd= (MaskModifierData *)md;
	DerivedMesh *dm= derivedData, *result= NULL;
	GHash *vertHash=NULL, *edgeHash, *faceHash;
	GHashIterator *hashIter;
	MDeformVert *dvert= NULL;
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
		GHash *vgroupHash, *boneHash;
		Object *oba= mmd->ob_arm;
		bPoseChannel *pchan;
		bDeformGroup *def;
		
		/* check that there is armature object with bones to use, otherwise return original mesh */
		if (ELEM(NULL, mmd->ob_arm, mmd->ob_arm->pose))
			return derivedData;		
		
		/* hashes for finding mapping of:
		 * 	- vgroups to indicies -> vgroupHash  (string, int)
		 *	- bones to vgroup indices -> boneHash (index of vgroup, dummy)
		 */
		vgroupHash= BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp);
		boneHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp);
		
		/* build mapping of names of vertex groups to indices */
		for (i = 0, def = ob->defbase.first; def; def = def->next, i++) 
			BLI_ghash_insert(vgroupHash, def->name, SET_INT_IN_POINTER(i));
		
		/* get selected-posechannel <-> vertexgroup index mapping */
		for (pchan= oba->pose->chanbase.first; pchan; pchan= pchan->next) 
		{
			/* check if bone is selected */
			// TODO: include checks for visibility too?
			// FIXME: the depsgraph needs extensions to make this work in realtime...
			if ( (pchan->bone) && (pchan->bone->flag & BONE_SELECTED) ) 
			{
				/* check if hash has group for this bone */
				if (BLI_ghash_haskey(vgroupHash, pchan->name)) 
				{
					int defgrp_index= GET_INT_FROM_POINTER(BLI_ghash_lookup(vgroupHash, pchan->name));
					
					/* add index to hash (store under key only) */
					BLI_ghash_insert(boneHash, SET_INT_IN_POINTER(defgrp_index), pchan);
				}
			}
		}
		
		/* if no bones selected, free hashes and return original mesh */
		if (BLI_ghash_size(boneHash) == 0)
		{
			BLI_ghash_free(vgroupHash, NULL, NULL);
			BLI_ghash_free(boneHash, NULL, NULL);
			
			return derivedData;
		}
		
		/* repeat the previous check, but for dverts */
		dvert= dm->getVertDataArray(dm, CD_MDEFORMVERT);
		if (dvert == NULL)
		{
			BLI_ghash_free(vgroupHash, NULL, NULL);
			BLI_ghash_free(boneHash, NULL, NULL);
			
			return derivedData;
		}
		
		/* hashes for quickly providing a mapping from old to new - use key=oldindex, value=newindex */
		vertHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp);
		
		/* add vertices which exist in vertexgroups into vertHash for filtering */
		for (i = 0; i < maxVerts; i++) 
		{
			MDeformWeight *def_weight = NULL;
			int j;
			
			for (j= 0; j < dvert[i].totweight; j++) 
			{
				if (BLI_ghash_haskey(boneHash, SET_INT_IN_POINTER(dvert[i].dw[j].def_nr))) 
				{
					def_weight = &dvert[i].dw[j];
					break;
				}
			}
			
			/* check if include vert in vertHash */
			if (mmd->flag & MOD_MASK_INV) {
				/* if this vert is in the vgroup, don't include it in vertHash */
				if (def_weight) continue;
			}
			else {
				/* if this vert isn't in the vgroup, don't include it in vertHash */
				if (!def_weight) continue;
			}
			
			/* add to ghash for verts (numVerts acts as counter for mapping) */
			BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numVerts));
			numVerts++;
		}
		
		/* free temp hashes */
		BLI_ghash_free(vgroupHash, NULL, NULL);
		BLI_ghash_free(boneHash, NULL, NULL);
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
		vertHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp);
		
		/* add vertices which exist in vertexgroup into ghash for filtering */
		for (i = 0; i < maxVerts; i++) 
		{
			MDeformWeight *def_weight = NULL;
			int j;
			
			for (j= 0; j < dvert[i].totweight; j++) 
			{
				if (dvert[i].dw[j].def_nr == defgrp_index) 
				{
					def_weight = &dvert[i].dw[j];
					break;
				}
			}
			
			/* check if include vert in vertHash */
			if (mmd->flag & MOD_MASK_INV) {
				/* if this vert is in the vgroup, don't include it in vertHash */
				if (def_weight) continue;
			}
			else {
				/* if this vert isn't in the vgroup, don't include it in vertHash */
				if (!def_weight) continue;
			}
			
			/* add to ghash for verts (numVerts acts as counter for mapping) */
			BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(i), SET_INT_IN_POINTER(numVerts));
			numVerts++;
		}
	}
	
	/* hashes for quickly providing a mapping from old to new - use key=oldindex, value=newindex */
	edgeHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp);
	faceHash= BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp);
	
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
	/* flags */             eModifierTypeFlag_AcceptsMesh,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   0,
	/* initData */          0,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
