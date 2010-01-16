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
* Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
* Modifier stack implementation.
*
* BKE_modifier.h contains the function prototypes for this file.
*
*/

#include "stddef.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_kdopbvh.h"
#include "BLI_kdtree.h"
#include "BLI_linklist.h"
#include "BLI_rand.h"
#include "BLI_edgehash.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_texture_types.h"

#include "BLI_editVert.h"




#include "BKE_main.h"
#include "BKE_anim.h"
#include "BKE_action.h"
#include "BKE_bmesh.h"
#include "BKE_booleanops.h"
#include "BKE_cloth.h"
#include "BKE_collision.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"
#include "BKE_simple_deform.h"

#include "LOD_decimation.h"

#include "CCGSubSurf.h"

#include "RE_shader_ext.h"

/* Utility */

static int is_last_displist(Object *ob)
{
	Curve *cu = ob->data;
	static int curvecount=0, totcurve=0;

	if(curvecount == 0){
		DispList *dl;

		totcurve = 0;
		for(dl=cu->disp.first; dl; dl=dl->next)
			totcurve++;
	}

	curvecount++;

	if(curvecount == totcurve){
		curvecount = 0;
		return 1;
	}

	return 0;
}

/* returns a derived mesh if dm == NULL, for deforming modifiers that need it */
static DerivedMesh *get_dm(Scene *scene, Object *ob, EditMesh *em, DerivedMesh *dm, float (*vertexCos)[3], int orco)
{
	if(dm)
		return dm;

	if(ob->type==OB_MESH) {
		if(em) dm= CDDM_from_editmesh(em, ob->data);
		else dm = CDDM_from_mesh((Mesh*)(ob->data), ob);

		if(vertexCos) {
			CDDM_apply_vert_coords(dm, vertexCos);
			//CDDM_calc_normals(dm);
		}
		
		if(orco)
			DM_add_vert_layer(dm, CD_ORCO, CD_ASSIGN, get_mesh_orco_verts(ob));
	}
	else if(ELEM3(ob->type,OB_FONT,OB_CURVE,OB_SURF)) {
		Object *tmpobj;
		Curve *tmpcu;

		if(is_last_displist(ob)) {
			/* copies object and modifiers (but not the data) */
			tmpobj= copy_object(ob);
			tmpcu = (Curve *)tmpobj->data;
			tmpcu->id.us--;

			/* copies the data */
			tmpobj->data = copy_curve((Curve *) ob->data);

			makeDispListCurveTypes(scene, tmpobj, 1);
			nurbs_to_mesh(tmpobj);

			dm = CDDM_from_mesh((Mesh*)(tmpobj->data), tmpobj);
			//CDDM_calc_normals(dm);

			free_libblock_us(&G.main->object, tmpobj);
		}
	}

	return dm;
}

/* returns a cdderivedmesh if dm == NULL or is another type of derivedmesh */
static DerivedMesh *get_cddm(Scene *scene, Object *ob, EditMesh *em, DerivedMesh *dm, float (*vertexCos)[3])
{
	if(dm && dm->type == DM_TYPE_CDDM)
		return dm;

	if(!dm) {
		dm= get_dm(scene, ob, em, dm, vertexCos, 0);
	}
	else {
		dm= CDDM_copy(dm);
		CDDM_apply_vert_coords(dm, vertexCos);
	}

	if(dm)
		CDDM_calc_normals(dm);
	
	return dm;
}

/***/

static int noneModifier_isDisabled(ModifierData *md, int userRenderParams)
{
	return 1;
}

/* Curve */

static void curveModifier_initData(ModifierData *md)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	cmd->defaxis = MOD_CURVE_POSX;
}

static void curveModifier_copyData(ModifierData *md, ModifierData *target)
{
	CurveModifierData *cmd = (CurveModifierData*) md;
	CurveModifierData *tcmd = (CurveModifierData*) target;

	tcmd->defaxis = cmd->defaxis;
	tcmd->object = cmd->object;
	strncpy(tcmd->name, cmd->name, 32);
}

static CustomDataMask curveModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	CurveModifierData *cmd = (CurveModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(cmd->name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static int curveModifier_isDisabled(ModifierData *md, int userRenderParams)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	return !cmd->object;
}

static void curveModifier_foreachObjectLink(
					    ModifierData *md, Object *ob,
	 void (*walk)(void *userData, Object *ob, Object **obpoin),
		void *userData)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	walk(userData, ob, &cmd->object);
}

static void curveModifier_updateDepgraph(
					 ModifierData *md, DagForest *forest, Scene *scene,
      Object *ob, DagNode *obNode)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	if (cmd->object) {
		DagNode *curNode = dag_get_node(forest, cmd->object);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Curve Modifier");
	}
}

static void curveModifier_deformVerts(
				      ModifierData *md, Object *ob, DerivedMesh *derivedData,
	  float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	CurveModifierData *cmd = (CurveModifierData*) md;

	curve_deform_verts(md->scene, cmd->object, ob, derivedData, vertexCos, numVerts,
			   cmd->name, cmd->defaxis);
}

static void curveModifier_deformVertsEM(
					ModifierData *md, Object *ob, EditMesh *editData,
     DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	curveModifier_deformVerts(md, ob, dm, vertexCos, numVerts, 0, 0);

	if(!derivedData) dm->release(dm);
}

/* Lattice */

static void latticeModifier_copyData(ModifierData *md, ModifierData *target)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;
	LatticeModifierData *tlmd = (LatticeModifierData*) target;

	tlmd->object = lmd->object;
	strncpy(tlmd->name, lmd->name, 32);
}

static CustomDataMask latticeModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	LatticeModifierData *lmd = (LatticeModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(lmd->name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static int latticeModifier_isDisabled(ModifierData *md, int userRenderParams)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	return !lmd->object;
}

static void latticeModifier_foreachObjectLink(
					      ModifierData *md, Object *ob,
	   void (*walk)(void *userData, Object *ob, Object **obpoin),
		  void *userData)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	walk(userData, ob, &lmd->object);
}

static void latticeModifier_updateDepgraph(ModifierData *md, DagForest *forest,  Scene *scene,
					   Object *ob, DagNode *obNode)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	if(lmd->object) {
		DagNode *latNode = dag_get_node(forest, lmd->object);

		dag_add_relation(forest, latNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Lattice Modifier");
	}
}

static void modifier_vgroup_cache(ModifierData *md, float (*vertexCos)[3])
{
	md= md->next;
	if(md) {
		if(md->type==eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData*) md;
			if(amd->multi)
				amd->prevCos= MEM_dupallocN(vertexCos);
		}
		/* lattice/mesh modifier too */
	}
}


static void latticeModifier_deformVerts(
					ModifierData *md, Object *ob, DerivedMesh *derivedData,
     float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;


	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */
	
	lattice_deform_verts(lmd->object, ob, derivedData,
			     vertexCos, numVerts, lmd->name);
}

static void latticeModifier_deformVertsEM(
					  ModifierData *md, Object *ob, EditMesh *editData,
       DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	latticeModifier_deformVerts(md, ob, dm, vertexCos, numVerts, 0, 0);

	if(!derivedData) dm->release(dm);
}

/* Subsurf */

static void subsurfModifier_initData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;

	smd->levels = 1;
	smd->renderLevels = 2;
	smd->flags |= eSubsurfModifierFlag_SubsurfUv;
}

static void subsurfModifier_copyData(ModifierData *md, ModifierData *target)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	SubsurfModifierData *tsmd = (SubsurfModifierData*) target;

	tsmd->flags = smd->flags;
	tsmd->levels = smd->levels;
	tsmd->renderLevels = smd->renderLevels;
	tsmd->subdivType = smd->subdivType;
}

static void subsurfModifier_freeData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;

	if(smd->mCache) {
		ccgSubSurf_free(smd->mCache);
	}
	if(smd->emCache) {
		ccgSubSurf_free(smd->emCache);
	}
}

static int subsurfModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;

	return (useRenderParams)? (smd->renderLevels == 0): (smd->levels == 0);
}

static DerivedMesh *subsurfModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	DerivedMesh *result;

	result = subsurf_make_derived_from_derived(derivedData, smd,
			useRenderParams, NULL, isFinalCalc, 0);
	
	if(useRenderParams || !isFinalCalc) {
		DerivedMesh *cddm= CDDM_copy(result);
		result->release(result);
		result= cddm;
	}

	return result;
}

static DerivedMesh *subsurfModifier_applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	SubsurfModifierData *smd = (SubsurfModifierData*) md;
	DerivedMesh *result;

	result = subsurf_make_derived_from_derived(derivedData, smd, 0,
			NULL, 0, 1);

	return result;
}

/* Build */

static void buildModifier_initData(ModifierData *md)
{
	BuildModifierData *bmd = (BuildModifierData*) md;

	bmd->start = 1.0;
	bmd->length = 100.0;
}

static void buildModifier_copyData(ModifierData *md, ModifierData *target)
{
	BuildModifierData *bmd = (BuildModifierData*) md;
	BuildModifierData *tbmd = (BuildModifierData*) target;

	tbmd->start = bmd->start;
	tbmd->length = bmd->length;
	tbmd->randomize = bmd->randomize;
	tbmd->seed = bmd->seed;
}

static int buildModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static DerivedMesh *buildModifier_applyModifier(ModifierData *md, Object *ob,
		DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	DerivedMesh *result;
	BuildModifierData *bmd = (BuildModifierData*) md;
	int i;
	int numFaces, numEdges;
	int maxVerts, maxEdges, maxFaces;
	int *vertMap, *edgeMap, *faceMap;
	float frac;
	GHashIterator *hashIter;
	/* maps vert indices in old mesh to indices in new mesh */
	GHash *vertHash = BLI_ghash_new(BLI_ghashutil_inthash,
					BLI_ghashutil_intcmp);
	/* maps edge indices in new mesh to indices in old mesh */
	GHash *edgeHash = BLI_ghash_new(BLI_ghashutil_inthash,
					BLI_ghashutil_intcmp);

	maxVerts = dm->getNumVerts(dm);
	vertMap = MEM_callocN(sizeof(*vertMap) * maxVerts,
			      "build modifier vertMap");
	for(i = 0; i < maxVerts; ++i) vertMap[i] = i;

	maxEdges = dm->getNumEdges(dm);
	edgeMap = MEM_callocN(sizeof(*edgeMap) * maxEdges,
			      "build modifier edgeMap");
	for(i = 0; i < maxEdges; ++i) edgeMap[i] = i;

	maxFaces = dm->getNumFaces(dm);
	faceMap = MEM_callocN(sizeof(*faceMap) * maxFaces,
			      "build modifier faceMap");
	for(i = 0; i < maxFaces; ++i) faceMap[i] = i;

	if (ob) {
		frac = bsystem_time(md->scene, ob, md->scene->r.cfra,
				    bmd->start - 1.0f) / bmd->length;
	} else {
		frac = md->scene->r.cfra - bmd->start / bmd->length;
	}
	CLAMP(frac, 0.0, 1.0);

	numFaces = dm->getNumFaces(dm) * frac;
	numEdges = dm->getNumEdges(dm) * frac;

	/* if there's at least one face, build based on faces */
	if(numFaces) {
		int maxEdges;

		if(bmd->randomize)
			BLI_array_randomize(faceMap, sizeof(*faceMap),
					    maxFaces, bmd->seed);

		/* get the set of all vert indices that will be in the final mesh,
		* mapped to the new indices
		*/
		for(i = 0; i < numFaces; ++i) {
			MFace mf;
			dm->getFace(dm, faceMap[i], &mf);

			if(!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v1)))
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(mf.v1),
					SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			if(!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v2)))
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(mf.v2),
					SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			if(!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v3)))
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(mf.v3),
					SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			if(mf.v4 && !BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(mf.v4)))
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(mf.v4),
					SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
		}

		/* get the set of edges that will be in the new mesh (i.e. all edges
		* that have both verts in the new mesh)
		*/
		maxEdges = dm->getNumEdges(dm);
		for(i = 0; i < maxEdges; ++i) {
			MEdge me;
			dm->getEdge(dm, i, &me);

			if(BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v1))
						&& BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v2)))
				BLI_ghash_insert(edgeHash,
					SET_INT_IN_POINTER(BLI_ghash_size(edgeHash)), SET_INT_IN_POINTER(i));
		}
	} else if(numEdges) {
		if(bmd->randomize)
			BLI_array_randomize(edgeMap, sizeof(*edgeMap),
					    maxEdges, bmd->seed);

		/* get the set of all vert indices that will be in the final mesh,
		* mapped to the new indices
		*/
		for(i = 0; i < numEdges; ++i) {
			MEdge me;
			dm->getEdge(dm, edgeMap[i], &me);

			if(!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v1)))
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(me.v1),
					SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
			if(!BLI_ghash_haskey(vertHash, SET_INT_IN_POINTER(me.v2)))
				BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(me.v2),
					SET_INT_IN_POINTER(BLI_ghash_size(vertHash)));
		}

		/* get the set of edges that will be in the new mesh
		*/
		for(i = 0; i < numEdges; ++i) {
			MEdge me;
			dm->getEdge(dm, edgeMap[i], &me);

			BLI_ghash_insert(edgeHash, SET_INT_IN_POINTER(BLI_ghash_size(edgeHash)),
					 SET_INT_IN_POINTER(edgeMap[i]));
		}
	} else {
		int numVerts = dm->getNumVerts(dm) * frac;

		if(bmd->randomize)
			BLI_array_randomize(vertMap, sizeof(*vertMap),
					    maxVerts, bmd->seed);

		/* get the set of all vert indices that will be in the final mesh,
		* mapped to the new indices
		*/
		for(i = 0; i < numVerts; ++i)
			BLI_ghash_insert(vertHash, SET_INT_IN_POINTER(vertMap[i]), SET_INT_IN_POINTER(i));
	}

	/* now we know the number of verts, edges and faces, we can create
	* the mesh
	*/
	result = CDDM_from_template(dm, BLI_ghash_size(vertHash),
				    BLI_ghash_size(edgeHash), numFaces);

	/* copy the vertices across */
	for(hashIter = BLI_ghashIterator_new(vertHash);
		   !BLI_ghashIterator_isDone(hashIter);
		   BLI_ghashIterator_step(hashIter)) {
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

		   /* copy the edges across, remapping indices */
		   for(i = 0; i < BLI_ghash_size(edgeHash); ++i) {
			   MEdge source;
			   MEdge *dest;
			   int oldIndex = GET_INT_FROM_POINTER(BLI_ghash_lookup(edgeHash, SET_INT_IN_POINTER(i)));

			   dm->getEdge(dm, oldIndex, &source);
			   dest = CDDM_get_edge(result, i);

			   source.v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v1)));
			   source.v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v2)));

			   DM_copy_edge_data(dm, result, oldIndex, i, 1);
			   *dest = source;
		   }

		   /* copy the faces across, remapping indices */
		   for(i = 0; i < numFaces; ++i) {
			   MFace source;
			   MFace *dest;
			   int orig_v4;

			   dm->getFace(dm, faceMap[i], &source);
			   dest = CDDM_get_face(result, i);

			   orig_v4 = source.v4;

			   source.v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v1)));
			   source.v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v2)));
			   source.v3 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v3)));
			   if(source.v4)
				   source.v4 = GET_INT_FROM_POINTER(BLI_ghash_lookup(vertHash, SET_INT_IN_POINTER(source.v4)));

			   DM_copy_face_data(dm, result, faceMap[i], i, 1);
			   *dest = source;

			   test_index_face(dest, &result->faceData, i, (orig_v4 ? 4 : 3));
		   }

		   CDDM_calc_normals(result);

		   BLI_ghash_free(vertHash, NULL, NULL);
		   BLI_ghash_free(edgeHash, NULL, NULL);

		   MEM_freeN(vertMap);
		   MEM_freeN(edgeMap);
		   MEM_freeN(faceMap);

		   return result;
}

/* Mask */

static void maskModifier_copyData(ModifierData *md, ModifierData *target)
{
	MaskModifierData *mmd = (MaskModifierData*) md;
	MaskModifierData *tmmd = (MaskModifierData*) target;
	
	strcpy(tmmd->vgroup, mmd->vgroup);
}

static CustomDataMask maskModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	return (1 << CD_MDEFORMVERT);
}

static void maskModifier_foreachObjectLink(
					      ModifierData *md, Object *ob,
	   void (*walk)(void *userData, Object *ob, Object **obpoin),
		  void *userData)
{
	MaskModifierData *mmd = (MaskModifierData *)md;
	walk(userData, ob, &mmd->ob_arm);
}

static void maskModifier_updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene,
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

static DerivedMesh *maskModifier_applyModifier(ModifierData *md, Object *ob,
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
		int defgrp_index = -1;
		
		/* get index of vertex group */
		if (mmd->vgroup[0]) 
		{
			bDeformGroup *def;
			
			/* find index by comparing names - SLOW... */
			for (i = 0, def = ob->defbase.first; def; def = def->next, i++) 
			{
				if (!strcmp(def->name, mmd->vgroup)) 
				{
					defgrp_index = i;
					break;
				}
			}
		}
		
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

/* Array */
/* Array modifier: duplicates the object multiple times along an axis
*/

static void arrayModifier_initData(ModifierData *md)
{
	ArrayModifierData *amd = (ArrayModifierData*) md;

	/* default to 2 duplicates distributed along the x-axis by an
	offset of 1 object-width
	*/
	amd->start_cap = amd->end_cap = amd->curve_ob = amd->offset_ob = NULL;
	amd->count = 2;
	amd->offset[0] = amd->offset[1] = amd->offset[2] = 0;
	amd->scale[0] = 1;
	amd->scale[1] = amd->scale[2] = 0;
	amd->length = 0;
	amd->merge_dist = 0.01;
	amd->fit_type = MOD_ARR_FIXEDCOUNT;
	amd->offset_type = MOD_ARR_OFF_RELATIVE;
	amd->flags = 0;
}

static void arrayModifier_copyData(ModifierData *md, ModifierData *target)
{
	ArrayModifierData *amd = (ArrayModifierData*) md;
	ArrayModifierData *tamd = (ArrayModifierData*) target;

	tamd->start_cap = amd->start_cap;
	tamd->end_cap = amd->end_cap;
	tamd->curve_ob = amd->curve_ob;
	tamd->offset_ob = amd->offset_ob;
	tamd->count = amd->count;
	VECCOPY(tamd->offset, amd->offset);
	VECCOPY(tamd->scale, amd->scale);
	tamd->length = amd->length;
	tamd->merge_dist = amd->merge_dist;
	tamd->fit_type = amd->fit_type;
	tamd->offset_type = amd->offset_type;
	tamd->flags = amd->flags;
}

static void arrayModifier_foreachObjectLink(
					    ModifierData *md, Object *ob,
	 void (*walk)(void *userData, Object *ob, Object **obpoin),
		void *userData)
{
	ArrayModifierData *amd = (ArrayModifierData*) md;

	walk(userData, ob, &amd->start_cap);
	walk(userData, ob, &amd->end_cap);
	walk(userData, ob, &amd->curve_ob);
	walk(userData, ob, &amd->offset_ob);
}

static void arrayModifier_updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene,
					 Object *ob, DagNode *obNode)
{
	ArrayModifierData *amd = (ArrayModifierData*) md;

	if (amd->start_cap) {
		DagNode *curNode = dag_get_node(forest, amd->start_cap);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->end_cap) {
		DagNode *curNode = dag_get_node(forest, amd->end_cap);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->curve_ob) {
		DagNode *curNode = dag_get_node(forest, amd->curve_ob);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
	if (amd->offset_ob) {
		DagNode *curNode = dag_get_node(forest, amd->offset_ob);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Array Modifier");
	}
}

static float vertarray_size(MVert *mvert, int numVerts, int axis)
{
	int i;
	float min_co, max_co;

	/* if there are no vertices, width is 0 */
	if(numVerts == 0) return 0;

	/* find the minimum and maximum coordinates on the desired axis */
	min_co = max_co = mvert->co[axis];
	++mvert;
	for(i = 1; i < numVerts; ++i, ++mvert) {
		if(mvert->co[axis] < min_co) min_co = mvert->co[axis];
		if(mvert->co[axis] > max_co) max_co = mvert->co[axis];
	}

	return max_co - min_co;
}

typedef struct IndexMapEntry {
	/* the new vert index that this old vert index maps to */
	int new;
	/* -1 if this vert isn't merged, otherwise the old vert index it
	* should be replaced with
	*/
	int merge;
	/* 1 if this vert's first copy is merged with the last copy of its
	* merge target, otherwise 0
	*/
	short merge_final;
} IndexMapEntry;

/* indexMap - an array of IndexMap entries
 * oldIndex - the old index to map
 * copyNum - the copy number to map to (original = 0, first copy = 1, etc.)
 */
static int calc_mapping(IndexMapEntry *indexMap, int oldIndex, int copyNum)
{
	if(indexMap[oldIndex].merge < 0) {
		/* vert wasn't merged, so use copy of this vert */
		return indexMap[oldIndex].new + copyNum;
	} else if(indexMap[oldIndex].merge == oldIndex) {
		/* vert was merged with itself */
		return indexMap[oldIndex].new;
	} else {
		/* vert was merged with another vert */
		/* follow the chain of merges to the end, or until we've passed
		* a number of vertices equal to the copy number
		*/
		if(copyNum <= 0)
			return indexMap[oldIndex].new;
		else
			return calc_mapping(indexMap, indexMap[oldIndex].merge,
					    copyNum - 1);
	}
}

static DerivedMesh *arrayModifier_doArray(ArrayModifierData *amd,
					  Scene *scene, Object *ob, DerivedMesh *dm,
       int initFlags)
{
	int i, j;
	/* offset matrix */
	float offset[4][4];
	float final_offset[4][4];
	float tmp_mat[4][4];
	float length = amd->length;
	int count = amd->count;
	int numVerts, numEdges, numFaces;
	int maxVerts, maxEdges, maxFaces;
	int finalVerts, finalEdges, finalFaces;
	DerivedMesh *result, *start_cap = NULL, *end_cap = NULL;
	MVert *mvert, *src_mvert;
	MEdge *medge;
	MFace *mface;

	IndexMapEntry *indexMap;

	EdgeHash *edges;

	/* need to avoid infinite recursion here */
	if(amd->start_cap && amd->start_cap != ob)
		start_cap = amd->start_cap->derivedFinal;
	if(amd->end_cap && amd->end_cap != ob)
		end_cap = amd->end_cap->derivedFinal;

	unit_m4(offset);

	indexMap = MEM_callocN(sizeof(*indexMap) * dm->getNumVerts(dm),
			       "indexmap");

	src_mvert = dm->getVertArray(dm);

	maxVerts = dm->getNumVerts(dm);

	if(amd->offset_type & MOD_ARR_OFF_CONST)
		add_v3_v3v3(offset[3], offset[3], amd->offset);
	if(amd->offset_type & MOD_ARR_OFF_RELATIVE) {
		for(j = 0; j < 3; j++)
			offset[3][j] += amd->scale[j] * vertarray_size(src_mvert,
					maxVerts, j);
	}

	if((amd->offset_type & MOD_ARR_OFF_OBJ) && (amd->offset_ob)) {
		float obinv[4][4];
		float result_mat[4][4];

		if(ob)
			invert_m4_m4(obinv, ob->obmat);
		else
			unit_m4(obinv);

		mul_serie_m4(result_mat, offset,
				 obinv, amd->offset_ob->obmat,
     NULL, NULL, NULL, NULL, NULL);
		copy_m4_m4(offset, result_mat);
	}

	if(amd->fit_type == MOD_ARR_FITCURVE && amd->curve_ob) {
		Curve *cu = amd->curve_ob->data;
		if(cu) {
			float tmp_mat[3][3];
			float scale;
			
			object_to_mat3(amd->curve_ob, tmp_mat);
			scale = mat3_to_scale(tmp_mat);
				
			if(!cu->path) {
				cu->flag |= CU_PATH; // needed for path & bevlist
				makeDispListCurveTypes(scene, amd->curve_ob, 0);
			}
			if(cu->path)
				length = scale*cu->path->totdist;
		}
	}

	/* calculate the maximum number of copies which will fit within the
	prescribed length */
	if(amd->fit_type == MOD_ARR_FITLENGTH
		  || amd->fit_type == MOD_ARR_FITCURVE) {
		float dist = sqrt(dot_v3v3(offset[3], offset[3]));

		if(dist > 1e-6f)
			/* this gives length = first copy start to last copy end
			add a tiny offset for floating point rounding errors */
			count = (length + 1e-6f) / dist;
		else
			/* if the offset has no translation, just make one copy */
			count = 1;
		  }

		  if(count < 1)
			  count = 1;

	/* allocate memory for count duplicates (including original) plus
		  * start and end caps
	*/
		  finalVerts = dm->getNumVerts(dm) * count;
		  finalEdges = dm->getNumEdges(dm) * count;
		  finalFaces = dm->getNumFaces(dm) * count;
		  if(start_cap) {
			  finalVerts += start_cap->getNumVerts(start_cap);
			  finalEdges += start_cap->getNumEdges(start_cap);
			  finalFaces += start_cap->getNumFaces(start_cap);
		  }
		  if(end_cap) {
			  finalVerts += end_cap->getNumVerts(end_cap);
			  finalEdges += end_cap->getNumEdges(end_cap);
			  finalFaces += end_cap->getNumFaces(end_cap);
		  }
		  result = CDDM_from_template(dm, finalVerts, finalEdges, finalFaces);

		  /* calculate the offset matrix of the final copy (for merging) */ 
		  unit_m4(final_offset);

		  for(j=0; j < count - 1; j++) {
			  mul_m4_m4m4(tmp_mat, final_offset, offset);
			  copy_m4_m4(final_offset, tmp_mat);
		  }

		  numVerts = numEdges = numFaces = 0;
		  mvert = CDDM_get_verts(result);

		  for (i = 0; i < maxVerts; i++) {
			  indexMap[i].merge = -1; /* default to no merge */
			  indexMap[i].merge_final = 0; /* default to no merge */
		  }

		  for (i = 0; i < maxVerts; i++) {
			  MVert *inMV;
			  MVert *mv = &mvert[numVerts];
			  MVert *mv2;
			  float co[3];

			  inMV = &src_mvert[i];

			  DM_copy_vert_data(dm, result, i, numVerts, 1);
			  *mv = *inMV;
			  numVerts++;

			  indexMap[i].new = numVerts - 1;

			  VECCOPY(co, mv->co);
		
		/* Attempts to merge verts from one duplicate with verts from the
			  * next duplicate which are closer than amd->merge_dist.
			  * Only the first such vert pair is merged.
			  * If verts are merged in the first duplicate pair, they are merged
			  * in all pairs.
		*/
			  if((count > 1) && (amd->flags & MOD_ARR_MERGE)) {
				  float tmp_co[3];
				  VECCOPY(tmp_co, mv->co);
				  mul_m4_v3(offset, tmp_co);

				  for(j = 0; j < maxVerts; j++) {
					  /* if vertex already merged, don't use it */
					  if( indexMap[j].merge != -1 ) continue;

					  inMV = &src_mvert[j];
					  /* if this vert is within merge limit, merge */
					  if(compare_len_v3v3(tmp_co, inMV->co, amd->merge_dist)) {
						  indexMap[i].merge = j;

						  /* test for merging with final copy of merge target */
						  if(amd->flags & MOD_ARR_MERGEFINAL) {
							  VECCOPY(tmp_co, inMV->co);
							  inMV = &src_mvert[i];
							  mul_m4_v3(final_offset, tmp_co);
							  if(compare_len_v3v3(tmp_co, inMV->co, amd->merge_dist))
								  indexMap[i].merge_final = 1;
						  }
						  break;
					  }
				  }
			  }

			  /* if no merging, generate copies of this vert */
			  if(indexMap[i].merge < 0) {
				  for(j=0; j < count - 1; j++) {
					  mv2 = &mvert[numVerts];

					  DM_copy_vert_data(result, result, numVerts - 1, numVerts, 1);
					  *mv2 = *mv;
					  numVerts++;

					  mul_m4_v3(offset, co);
					  VECCOPY(mv2->co, co);
				  }
			  } else if(indexMap[i].merge != i && indexMap[i].merge_final) {
			/* if this vert is not merging with itself, and it is merging
				  * with the final copy of its merge target, remove the first copy
			*/
				  numVerts--;
				  DM_free_vert_data(result, numVerts, 1);
			  }
		  }

		  /* make a hashtable so we can avoid duplicate edges from merging */
		  edges = BLI_edgehash_new();

		  maxEdges = dm->getNumEdges(dm);
		  medge = CDDM_get_edges(result);
		  for(i = 0; i < maxEdges; i++) {
			  MEdge inMED;
			  MEdge med;
			  MEdge *med2;
			  int vert1, vert2;

			  dm->getEdge(dm, i, &inMED);

			  med = inMED;
			  med.v1 = indexMap[inMED.v1].new;
			  med.v2 = indexMap[inMED.v2].new;

		/* if vertices are to be merged with the final copies of their
			  * merge targets, calculate that final copy
		*/
			  if(indexMap[inMED.v1].merge_final) {
				  med.v1 = calc_mapping(indexMap, indexMap[inMED.v1].merge,
						  count - 1);
			  }
			  if(indexMap[inMED.v2].merge_final) {
				  med.v2 = calc_mapping(indexMap, indexMap[inMED.v2].merge,
						  count - 1);
			  }

			  if(med.v1 == med.v2) continue;

			  if (initFlags) {
				  med.flag |= ME_EDGEDRAW | ME_EDGERENDER;
			  }

			  if(!BLI_edgehash_haskey(edges, med.v1, med.v2)) {
				  DM_copy_edge_data(dm, result, i, numEdges, 1);
				  medge[numEdges] = med;
				  numEdges++;

				  BLI_edgehash_insert(edges, med.v1, med.v2, NULL);
			  }

			  for(j = 1; j < count; j++)
			  {
				  vert1 = calc_mapping(indexMap, inMED.v1, j);
				  vert2 = calc_mapping(indexMap, inMED.v2, j);
				  /* avoid duplicate edges */
				  if(!BLI_edgehash_haskey(edges, vert1, vert2)) {
					  med2 = &medge[numEdges];

					  DM_copy_edge_data(dm, result, i, numEdges, 1);
					  *med2 = med;
					  numEdges++;

					  med2->v1 = vert1;
					  med2->v2 = vert2;

					  BLI_edgehash_insert(edges, med2->v1, med2->v2, NULL);
				  }
			  }
		  }

		  maxFaces = dm->getNumFaces(dm);
		  mface = CDDM_get_faces(result);
		  for (i=0; i < maxFaces; i++) {
			  MFace inMF;
			  MFace *mf = &mface[numFaces];

			  dm->getFace(dm, i, &inMF);

			  DM_copy_face_data(dm, result, i, numFaces, 1);
			  *mf = inMF;

			  mf->v1 = indexMap[inMF.v1].new;
			  mf->v2 = indexMap[inMF.v2].new;
			  mf->v3 = indexMap[inMF.v3].new;
			  if(inMF.v4)
				  mf->v4 = indexMap[inMF.v4].new;

		/* if vertices are to be merged with the final copies of their
			  * merge targets, calculate that final copy
		*/
			  if(indexMap[inMF.v1].merge_final)
				  mf->v1 = calc_mapping(indexMap, indexMap[inMF.v1].merge, count-1);
			  if(indexMap[inMF.v2].merge_final)
				  mf->v2 = calc_mapping(indexMap, indexMap[inMF.v2].merge, count-1);
			  if(indexMap[inMF.v3].merge_final)
				  mf->v3 = calc_mapping(indexMap, indexMap[inMF.v3].merge, count-1);
			  if(inMF.v4 && indexMap[inMF.v4].merge_final)
				  mf->v4 = calc_mapping(indexMap, indexMap[inMF.v4].merge, count-1);

			  if(test_index_face(mf, &result->faceData, numFaces, inMF.v4?4:3) < 3)
				  continue;

			  numFaces++;

			  /* if the face has fewer than 3 vertices, don't create it */
			  if(mf->v3 == 0 || (mf->v1 && (mf->v1 == mf->v3 || mf->v1 == mf->v4))) {
				  numFaces--;
				  DM_free_face_data(result, numFaces, 1);
			  }

			  for(j = 1; j < count; j++)
			  {
				  MFace *mf2 = &mface[numFaces];

				  DM_copy_face_data(dm, result, i, numFaces, 1);
				  *mf2 = *mf;

				  mf2->v1 = calc_mapping(indexMap, inMF.v1, j);
				  mf2->v2 = calc_mapping(indexMap, inMF.v2, j);
				  mf2->v3 = calc_mapping(indexMap, inMF.v3, j);
				  if (inMF.v4)
					  mf2->v4 = calc_mapping(indexMap, inMF.v4, j);

				  test_index_face(mf2, &result->faceData, numFaces, inMF.v4?4:3);
				  numFaces++;

				  /* if the face has fewer than 3 vertices, don't create it */
				  if(mf2->v3 == 0 || (mf2->v1 && (mf2->v1 == mf2->v3 || mf2->v1 ==
								 mf2->v4))) {
					  numFaces--;
					  DM_free_face_data(result, numFaces, 1);
								 }
			  }
		  }

		  /* add start and end caps */
		  if(start_cap) {
			  float startoffset[4][4];
			  MVert *cap_mvert;
			  MEdge *cap_medge;
			  MFace *cap_mface;
			  int *origindex;
			  int *vert_map;
			  int capVerts, capEdges, capFaces;

			  capVerts = start_cap->getNumVerts(start_cap);
			  capEdges = start_cap->getNumEdges(start_cap);
			  capFaces = start_cap->getNumFaces(start_cap);
			  cap_mvert = start_cap->getVertArray(start_cap);
			  cap_medge = start_cap->getEdgeArray(start_cap);
			  cap_mface = start_cap->getFaceArray(start_cap);

			  invert_m4_m4(startoffset, offset);

			  vert_map = MEM_callocN(sizeof(*vert_map) * capVerts,
					  "arrayModifier_doArray vert_map");

			  origindex = result->getVertDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capVerts; i++) {
				  MVert *mv = &cap_mvert[i];
				  short merged = 0;

				  if(amd->flags & MOD_ARR_MERGE) {
					  float tmp_co[3];
					  MVert *in_mv;
					  int j;

					  VECCOPY(tmp_co, mv->co);
					  mul_m4_v3(startoffset, tmp_co);

					  for(j = 0; j < maxVerts; j++) {
						  in_mv = &src_mvert[j];
						  /* if this vert is within merge limit, merge */
						  if(compare_len_v3v3(tmp_co, in_mv->co, amd->merge_dist)) {
							  vert_map[i] = calc_mapping(indexMap, j, 0);
							  merged = 1;
							  break;
						  }
					  }
				  }

				  if(!merged) {
					  DM_copy_vert_data(start_cap, result, i, numVerts, 1);
					  mvert[numVerts] = *mv;
					  mul_m4_v3(startoffset, mvert[numVerts].co);
					  origindex[numVerts] = ORIGINDEX_NONE;

					  vert_map[i] = numVerts;

					  numVerts++;
				  }
			  }
			  origindex = result->getEdgeDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capEdges; i++) {
				  int v1, v2;

				  v1 = vert_map[cap_medge[i].v1];
				  v2 = vert_map[cap_medge[i].v2];

				  if(!BLI_edgehash_haskey(edges, v1, v2)) {
					  DM_copy_edge_data(start_cap, result, i, numEdges, 1);
					  medge[numEdges] = cap_medge[i];
					  medge[numEdges].v1 = v1;
					  medge[numEdges].v2 = v2;
					  origindex[numEdges] = ORIGINDEX_NONE;

					  numEdges++;
				  }
			  }
			  origindex = result->getFaceDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capFaces; i++) {
				  DM_copy_face_data(start_cap, result, i, numFaces, 1);
				  mface[numFaces] = cap_mface[i];
				  mface[numFaces].v1 = vert_map[mface[numFaces].v1];
				  mface[numFaces].v2 = vert_map[mface[numFaces].v2];
				  mface[numFaces].v3 = vert_map[mface[numFaces].v3];
				  if(mface[numFaces].v4) {
					  mface[numFaces].v4 = vert_map[mface[numFaces].v4];

					  test_index_face(&mface[numFaces], &result->faceData,
					                  numFaces, 4);
				  }
				  else
				  {
					  test_index_face(&mface[numFaces], &result->faceData,
					                  numFaces, 3);
				  }

				  origindex[numFaces] = ORIGINDEX_NONE;

				  numFaces++;
			  }

			  MEM_freeN(vert_map);
			  start_cap->release(start_cap);
		  }

		  if(end_cap) {
			  float endoffset[4][4];
			  MVert *cap_mvert;
			  MEdge *cap_medge;
			  MFace *cap_mface;
			  int *origindex;
			  int *vert_map;
			  int capVerts, capEdges, capFaces;

			  capVerts = end_cap->getNumVerts(end_cap);
			  capEdges = end_cap->getNumEdges(end_cap);
			  capFaces = end_cap->getNumFaces(end_cap);
			  cap_mvert = end_cap->getVertArray(end_cap);
			  cap_medge = end_cap->getEdgeArray(end_cap);
			  cap_mface = end_cap->getFaceArray(end_cap);

			  mul_m4_m4m4(endoffset, final_offset, offset);

			  vert_map = MEM_callocN(sizeof(*vert_map) * capVerts,
					  "arrayModifier_doArray vert_map");

			  origindex = result->getVertDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capVerts; i++) {
				  MVert *mv = &cap_mvert[i];
				  short merged = 0;

				  if(amd->flags & MOD_ARR_MERGE) {
					  float tmp_co[3];
					  MVert *in_mv;
					  int j;

					  VECCOPY(tmp_co, mv->co);
					  mul_m4_v3(offset, tmp_co);

					  for(j = 0; j < maxVerts; j++) {
						  in_mv = &src_mvert[j];
						  /* if this vert is within merge limit, merge */
						  if(compare_len_v3v3(tmp_co, in_mv->co, amd->merge_dist)) {
							  vert_map[i] = calc_mapping(indexMap, j, count - 1);
							  merged = 1;
							  break;
						  }
					  }
				  }

				  if(!merged) {
					  DM_copy_vert_data(end_cap, result, i, numVerts, 1);
					  mvert[numVerts] = *mv;
					  mul_m4_v3(endoffset, mvert[numVerts].co);
					  origindex[numVerts] = ORIGINDEX_NONE;

					  vert_map[i] = numVerts;

					  numVerts++;
				  }
			  }
			  origindex = result->getEdgeDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capEdges; i++) {
				  int v1, v2;

				  v1 = vert_map[cap_medge[i].v1];
				  v2 = vert_map[cap_medge[i].v2];

				  if(!BLI_edgehash_haskey(edges, v1, v2)) {
					  DM_copy_edge_data(end_cap, result, i, numEdges, 1);
					  medge[numEdges] = cap_medge[i];
					  medge[numEdges].v1 = v1;
					  medge[numEdges].v2 = v2;
					  origindex[numEdges] = ORIGINDEX_NONE;

					  numEdges++;
				  }
			  }
			  origindex = result->getFaceDataArray(result, CD_ORIGINDEX);
			  for(i = 0; i < capFaces; i++) {
				  DM_copy_face_data(end_cap, result, i, numFaces, 1);
				  mface[numFaces] = cap_mface[i];
				  mface[numFaces].v1 = vert_map[mface[numFaces].v1];
				  mface[numFaces].v2 = vert_map[mface[numFaces].v2];
				  mface[numFaces].v3 = vert_map[mface[numFaces].v3];
				  if(mface[numFaces].v4) {
					  mface[numFaces].v4 = vert_map[mface[numFaces].v4];

					  test_index_face(&mface[numFaces], &result->faceData,
					                  numFaces, 4);
				  }
				  else
				  {
					  test_index_face(&mface[numFaces], &result->faceData,
					                  numFaces, 3);
				  }
				  origindex[numFaces] = ORIGINDEX_NONE;

				  numFaces++;
			  }

			  MEM_freeN(vert_map);
			  end_cap->release(end_cap);
		  }

		  BLI_edgehash_free(edges, NULL);
		  MEM_freeN(indexMap);

		  CDDM_lower_num_verts(result, numVerts);
		  CDDM_lower_num_edges(result, numEdges);
		  CDDM_lower_num_faces(result, numFaces);

		  return result;
}

static DerivedMesh *arrayModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	ArrayModifierData *amd = (ArrayModifierData*) md;

	result = arrayModifier_doArray(amd, md->scene, ob, derivedData, 0);

	if(result != derivedData)
		CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *arrayModifier_applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return arrayModifier_applyModifier(md, ob, derivedData, 0, 1);
}

/* Mirror */

static void mirrorModifier_initData(ModifierData *md)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	mmd->flag |= (MOD_MIR_AXIS_X | MOD_MIR_VGROUP);
	mmd->tolerance = 0.001;
	mmd->mirror_ob = NULL;
}

static void mirrorModifier_copyData(ModifierData *md, ModifierData *target)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;
	MirrorModifierData *tmmd = (MirrorModifierData*) target;

	tmmd->axis = mmd->axis;
	tmmd->flag = mmd->flag;
	tmmd->tolerance = mmd->tolerance;
	tmmd->mirror_ob = mmd->mirror_ob;;
}

static void mirrorModifier_foreachObjectLink(
					     ModifierData *md, Object *ob,
	  void (*walk)(void *userData, Object *ob, Object **obpoin),
		 void *userData)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	walk(userData, ob, &mmd->mirror_ob);
}

static void mirrorModifier_updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene,
					  Object *ob, DagNode *obNode)
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
		int initFlags,
		int axis)
{
	int i;
	float tolerance = mmd->tolerance;
	DerivedMesh *result;
	int numVerts, numEdges, numFaces;
	int maxVerts = dm->getNumVerts(dm);
	int maxEdges = dm->getNumEdges(dm);
	int maxFaces = dm->getNumFaces(dm);
	int *flip_map= NULL;
	int do_vgroup_mirr= (mmd->flag & MOD_MIR_VGROUP);
	int (*indexMap)[2];
	float mtx[4][4], imtx[4][4];

	numVerts = numEdges = numFaces = 0;

	indexMap = MEM_mallocN(sizeof(*indexMap) * maxVerts, "indexmap");

	result = CDDM_from_template(dm, maxVerts * 2, maxEdges * 2, maxFaces * 2);


	if (do_vgroup_mirr) {
		flip_map= get_defgroup_flip_map(ob);
		if(flip_map == NULL)
			do_vgroup_mirr= 0;
	}

	if (mmd->mirror_ob) {
		float obinv[4][4];
		
		invert_m4_m4(obinv, mmd->mirror_ob->obmat);
		mul_m4_m4m4(mtx, ob->obmat, obinv);
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
			mul_v3_m4v3(co, mtx, co);
		}
		isShared = ABS(co[axis])<=tolerance;
		
		/* Because the topology result (# of vertices) must be the same if
		* the mesh data is overridden by vertex cos, have to calc sharedness
		* based on original coordinates. This is why we test before copy.
		*/
		DM_copy_vert_data(dm, result, i, numVerts, 1);
		*mv = inMV;
		numVerts++;
		
		indexMap[i][0] = numVerts - 1;
		indexMap[i][1] = !isShared;
		
		if(isShared) {
			co[axis] = 0;
			if (mmd->mirror_ob) {
				mul_v3_m4v3(co, imtx, co);
			}
			copy_v3_v3(mv->co, co);
			
			mv->flag |= ME_VERT_MERGED;
		} else {
			MVert *mv2 = CDDM_get_vert(result, numVerts);
			
			DM_copy_vert_data(dm, result, i, numVerts, 1);
			*mv2 = *mv;
			
			co[axis] = -co[axis];
			if (mmd->mirror_ob) {
				mul_v3_m4v3(co, imtx, co);
			}
			copy_v3_v3(mv2->co, co);
			
			if (do_vgroup_mirr) {
				MDeformVert *dvert= DM_get_vert_data(result, numVerts, CD_MDEFORMVERT);
				if(dvert) {
					flip_defvert(dvert, flip_map);
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
		if(initFlags)
			med->flag |= ME_EDGEDRAW | ME_EDGERENDER;
		
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
		
		if(indexMap[inMF.v1][1]
				 || indexMap[inMF.v2][1]
				 || indexMap[inMF.v3][1]
				 || (mf->v4 && indexMap[inMF.v4][1])) {
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
			SWAP(int, mf2->v1, mf2->v3);
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
					    Object *ob, DerivedMesh *dm,
						int initFlags)
{
	DerivedMesh *result = dm;

	/* check which axes have been toggled and mirror accordingly */
	if(mmd->flag & MOD_MIR_AXIS_X) {
		result = doMirrorOnAxis(mmd, ob, result, initFlags, 0);
	}
	if(mmd->flag & MOD_MIR_AXIS_Y) {
		DerivedMesh *tmp = result;
		result = doMirrorOnAxis(mmd, ob, result, initFlags, 1);
		if(tmp != dm) tmp->release(tmp); /* free intermediate results */
	}
	if(mmd->flag & MOD_MIR_AXIS_Z) {
		DerivedMesh *tmp = result;
		result = doMirrorOnAxis(mmd, ob, result, initFlags, 2);
		if(tmp != dm) tmp->release(tmp); /* free intermediate results */
	}

	return result;
}

static DerivedMesh *mirrorModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	result = mirrorModifier__doMirror(mmd, ob, derivedData, 0);

	if(result != derivedData)
		CDDM_calc_normals(result);
	
	return result;
}

static DerivedMesh *mirrorModifier_applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return mirrorModifier_applyModifier(md, ob, derivedData, 0, 1);
}

/* EdgeSplit */
/* EdgeSplit modifier: Splits edges in the mesh according to sharpness flag
 * or edge angle (can be used to achieve autosmoothing)
*/
#if 0
#define EDGESPLIT_DEBUG_3
#define EDGESPLIT_DEBUG_2
#define EDGESPLIT_DEBUG_1
#define EDGESPLIT_DEBUG_0
#endif

static void edgesplitModifier_initData(ModifierData *md)
{
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;

	/* default to 30-degree split angle, sharpness from both angle & flag
	*/
	emd->split_angle = 30;
	emd->flags = MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG;
}

static void edgesplitModifier_copyData(ModifierData *md, ModifierData *target)
{
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;
	EdgeSplitModifierData *temd = (EdgeSplitModifierData*) target;

	temd->split_angle = emd->split_angle;
	temd->flags = emd->flags;
}

/* Mesh data for edgesplit operation */
typedef struct SmoothVert {
	LinkNode *faces;     /* all faces which use this vert */
	int oldIndex; /* the index of the original DerivedMesh vert */
	int newIndex; /* the index of the new DerivedMesh vert */
} SmoothVert;

#define SMOOTHEDGE_NUM_VERTS 2

typedef struct SmoothEdge {
	SmoothVert *verts[SMOOTHEDGE_NUM_VERTS]; /* the verts used by this edge */
	LinkNode *faces;     /* all faces which use this edge */
	int oldIndex; /* the index of the original DerivedMesh edge */
	int newIndex; /* the index of the new DerivedMesh edge */
	short flag; /* the flags from the original DerivedMesh edge */
} SmoothEdge;

#define SMOOTHFACE_MAX_EDGES 4

typedef struct SmoothFace {
	SmoothEdge *edges[SMOOTHFACE_MAX_EDGES]; /* nonexistent edges == NULL */
	int flip[SMOOTHFACE_MAX_EDGES]; /* 1 = flip edge dir, 0 = don't flip */
	float normal[3]; /* the normal of this face */
	int oldIndex; /* the index of the original DerivedMesh face */
	int newIndex; /* the index of the new DerivedMesh face */
} SmoothFace;

typedef struct SmoothMesh {
	SmoothVert *verts;
	SmoothEdge *edges;
	SmoothFace *faces;
	int num_verts, num_edges, num_faces;
	int max_verts, max_edges, max_faces;
	DerivedMesh *dm;
	float threshold; /* the cosine of the smoothing angle */
	int flags;
	MemArena *arena;
	ListBase propagatestack, reusestack;
} SmoothMesh;

static SmoothVert *smoothvert_copy(SmoothVert *vert, SmoothMesh *mesh)
{
	SmoothVert *copy = &mesh->verts[mesh->num_verts];

	if(mesh->num_verts >= mesh->max_verts) {
		printf("Attempted to add a SmoothMesh vert beyond end of array\n");
		return NULL;
	}

	*copy = *vert;
	copy->faces = NULL;
	copy->newIndex = mesh->num_verts;
	++mesh->num_verts;

#ifdef EDGESPLIT_DEBUG_2
	printf("copied vert %4d to vert %4d\n", vert->newIndex, copy->newIndex);
#endif
	return copy;
}

static SmoothEdge *smoothedge_copy(SmoothEdge *edge, SmoothMesh *mesh)
{
	SmoothEdge *copy = &mesh->edges[mesh->num_edges];

	if(mesh->num_edges >= mesh->max_edges) {
		printf("Attempted to add a SmoothMesh edge beyond end of array\n");
		return NULL;
	}

	*copy = *edge;
	copy->faces = NULL;
	copy->newIndex = mesh->num_edges;
	++mesh->num_edges;

#ifdef EDGESPLIT_DEBUG_2
	printf("copied edge %4d to edge %4d\n", edge->newIndex, copy->newIndex);
#endif
	return copy;
}

static int smoothedge_has_vert(SmoothEdge *edge, SmoothVert *vert)
{
	int i;
	for(i = 0; i < SMOOTHEDGE_NUM_VERTS; i++)
		if(edge->verts[i] == vert) return 1;

	return 0;
}

static SmoothMesh *smoothmesh_new(int num_verts, int num_edges, int num_faces,
				  int max_verts, int max_edges, int max_faces)
{
	SmoothMesh *mesh = MEM_callocN(sizeof(*mesh), "smoothmesh");
	mesh->verts = MEM_callocN(sizeof(*mesh->verts) * max_verts,
				  "SmoothMesh.verts");
	mesh->edges = MEM_callocN(sizeof(*mesh->edges) * max_edges,
				  "SmoothMesh.edges");
	mesh->faces = MEM_callocN(sizeof(*mesh->faces) * max_faces,
				  "SmoothMesh.faces");

	mesh->num_verts = num_verts;
	mesh->num_edges = num_edges;
	mesh->num_faces = num_faces;

	mesh->max_verts = max_verts;
	mesh->max_edges = max_edges;
	mesh->max_faces = max_faces;

	return mesh;
}

static void smoothmesh_free(SmoothMesh *mesh)
{
	int i;

	for(i = 0; i < mesh->num_verts; ++i)
		BLI_linklist_free(mesh->verts[i].faces, NULL);

	for(i = 0; i < mesh->num_edges; ++i)
		BLI_linklist_free(mesh->edges[i].faces, NULL);
	
	if(mesh->arena)
		BLI_memarena_free(mesh->arena);

	MEM_freeN(mesh->verts);
	MEM_freeN(mesh->edges);
	MEM_freeN(mesh->faces);
	MEM_freeN(mesh);
}

static void smoothmesh_resize_verts(SmoothMesh *mesh, int max_verts)
{
	int i;
	SmoothVert *tmp;

	if(max_verts <= mesh->max_verts) return;

	tmp = MEM_callocN(sizeof(*tmp) * max_verts, "SmoothMesh.verts");

	memcpy(tmp, mesh->verts, sizeof(*tmp) * mesh->num_verts);

	/* remap vert pointers in edges */
	for(i = 0; i < mesh->num_edges; ++i) {
		int j;
		SmoothEdge *edge = &mesh->edges[i];

		for(j = 0; j < SMOOTHEDGE_NUM_VERTS; ++j)
			/* pointer arithmetic to get vert array index */
			edge->verts[j] = &tmp[edge->verts[j] - mesh->verts];
	}

	MEM_freeN(mesh->verts);
	mesh->verts = tmp;
	mesh->max_verts = max_verts;
}

static void smoothmesh_resize_edges(SmoothMesh *mesh, int max_edges)
{
	int i;
	SmoothEdge *tmp;

	if(max_edges <= mesh->max_edges) return;

	tmp = MEM_callocN(sizeof(*tmp) * max_edges, "SmoothMesh.edges");

	memcpy(tmp, mesh->edges, sizeof(*tmp) * mesh->num_edges);

	/* remap edge pointers in faces */
	for(i = 0; i < mesh->num_faces; ++i) {
		int j;
		SmoothFace *face = &mesh->faces[i];

		for(j = 0; j < SMOOTHFACE_MAX_EDGES; ++j)
			if(face->edges[j])
				/* pointer arithmetic to get edge array index */
				face->edges[j] = &tmp[face->edges[j] - mesh->edges];
	}

	MEM_freeN(mesh->edges);
	mesh->edges = tmp;
	mesh->max_edges = max_edges;
}

#ifdef EDGESPLIT_DEBUG_0
static void smoothmesh_print(SmoothMesh *mesh)
{
	int i, j;
	DerivedMesh *dm = mesh->dm;

	printf("--- SmoothMesh ---\n");
	printf("--- Vertices ---\n");
	for(i = 0; i < mesh->num_verts; i++) {
		SmoothVert *vert = &mesh->verts[i];
		LinkNode *node;
		MVert mv;

		dm->getVert(dm, vert->oldIndex, &mv);

		printf("%3d: ind={%3d, %3d}, pos={% 5.1f, % 5.1f, % 5.1f}",
		       i, vert->oldIndex, vert->newIndex,
	 mv.co[0], mv.co[1], mv.co[2]);
		printf(", faces={");
		for(node = vert->faces; node != NULL; node = node->next) {
			printf(" %d", ((SmoothFace *)node->link)->newIndex);
		}
		printf("}\n");
	}

	printf("\n--- Edges ---\n");
	for(i = 0; i < mesh->num_edges; i++) {
		SmoothEdge *edge = &mesh->edges[i];
		LinkNode *node;

		printf("%4d: indices={%4d, %4d}, verts={%4d, %4d}",
		       i,
	 edge->oldIndex, edge->newIndex,
  edge->verts[0]->newIndex, edge->verts[1]->newIndex);
		if(edge->verts[0] == edge->verts[1]) printf(" <- DUPLICATE VERTEX");
		printf(", faces={");
		for(node = edge->faces; node != NULL; node = node->next) {
			printf(" %d", ((SmoothFace *)node->link)->newIndex);
		}
		printf("}\n");
	}

	printf("\n--- Faces ---\n");
	for(i = 0; i < mesh->num_faces; i++) {
		SmoothFace *face = &mesh->faces[i];

		printf("%4d: indices={%4d, %4d}, edges={", i,
		       face->oldIndex, face->newIndex);
		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			if(face->flip[j])
				printf(" -%-2d", face->edges[j]->newIndex);
			else
				printf("  %-2d", face->edges[j]->newIndex);
		}
		printf("}, verts={");
		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			printf(" %d", face->edges[j]->verts[face->flip[j]]->newIndex);
		}
		printf("}\n");
	}
}
#endif

static SmoothMesh *smoothmesh_from_derivedmesh(DerivedMesh *dm)
{
	SmoothMesh *mesh;
	EdgeHash *edges = BLI_edgehash_new();
	int i;
	int totvert, totedge, totface;

	totvert = dm->getNumVerts(dm);
	totedge = dm->getNumEdges(dm);
	totface = dm->getNumFaces(dm);

	mesh = smoothmesh_new(totvert, totedge, totface,
			      totvert, totedge, totface);

	mesh->dm = dm;

	for(i = 0; i < totvert; i++) {
		SmoothVert *vert = &mesh->verts[i];

		vert->oldIndex = vert->newIndex = i;
	}

	for(i = 0; i < totedge; i++) {
		SmoothEdge *edge = &mesh->edges[i];
		MEdge med;

		dm->getEdge(dm, i, &med);
		edge->verts[0] = &mesh->verts[med.v1];
		edge->verts[1] = &mesh->verts[med.v2];
		edge->oldIndex = edge->newIndex = i;
		edge->flag = med.flag;

		BLI_edgehash_insert(edges, med.v1, med.v2, edge);
	}

	for(i = 0; i < totface; i++) {
		SmoothFace *face = &mesh->faces[i];
		MFace mf;
		MVert v1, v2, v3;
		int j;

		dm->getFace(dm, i, &mf);

		dm->getVert(dm, mf.v1, &v1);
		dm->getVert(dm, mf.v2, &v2);
		dm->getVert(dm, mf.v3, &v3);
		face->edges[0] = BLI_edgehash_lookup(edges, mf.v1, mf.v2);
		if(face->edges[0]->verts[1]->oldIndex == mf.v1) face->flip[0] = 1;
		face->edges[1] = BLI_edgehash_lookup(edges, mf.v2, mf.v3);
		if(face->edges[1]->verts[1]->oldIndex == mf.v2) face->flip[1] = 1;
		if(mf.v4) {
			MVert v4;
			dm->getVert(dm, mf.v4, &v4);
			face->edges[2] = BLI_edgehash_lookup(edges, mf.v3, mf.v4);
			if(face->edges[2]->verts[1]->oldIndex == mf.v3) face->flip[2] = 1;
			face->edges[3] = BLI_edgehash_lookup(edges, mf.v4, mf.v1);
			if(face->edges[3]->verts[1]->oldIndex == mf.v4) face->flip[3] = 1;
			normal_quad_v3( face->normal,v1.co, v2.co, v3.co, v4.co);
		} else {
			face->edges[2] = BLI_edgehash_lookup(edges, mf.v3, mf.v1);
			if(face->edges[2]->verts[1]->oldIndex == mf.v3) face->flip[2] = 1;
			face->edges[3] = NULL;
			normal_tri_v3( face->normal,v1.co, v2.co, v3.co);
		}

		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			SmoothEdge *edge = face->edges[j];
			BLI_linklist_prepend(&edge->faces, face);
			BLI_linklist_prepend(&edge->verts[face->flip[j]]->faces, face);
		}

		face->oldIndex = face->newIndex = i;
	}

	BLI_edgehash_free(edges, NULL);

	return mesh;
}

static DerivedMesh *CDDM_from_smoothmesh(SmoothMesh *mesh)
{
	DerivedMesh *result = CDDM_from_template(mesh->dm,
			mesh->num_verts,
   mesh->num_edges,
   mesh->num_faces);
	MVert *new_verts = CDDM_get_verts(result);
	MEdge *new_edges = CDDM_get_edges(result);
	MFace *new_faces = CDDM_get_faces(result);
	int i;

	for(i = 0; i < mesh->num_verts; ++i) {
		SmoothVert *vert = &mesh->verts[i];
		MVert *newMV = &new_verts[vert->newIndex];

		DM_copy_vert_data(mesh->dm, result,
				  vert->oldIndex, vert->newIndex, 1);
		mesh->dm->getVert(mesh->dm, vert->oldIndex, newMV);
	}

	for(i = 0; i < mesh->num_edges; ++i) {
		SmoothEdge *edge = &mesh->edges[i];
		MEdge *newME = &new_edges[edge->newIndex];

		DM_copy_edge_data(mesh->dm, result,
				  edge->oldIndex, edge->newIndex, 1);
		mesh->dm->getEdge(mesh->dm, edge->oldIndex, newME);
		newME->v1 = edge->verts[0]->newIndex;
		newME->v2 = edge->verts[1]->newIndex;
	}

	for(i = 0; i < mesh->num_faces; ++i) {
		SmoothFace *face = &mesh->faces[i];
		MFace *newMF = &new_faces[face->newIndex];

		DM_copy_face_data(mesh->dm, result,
				  face->oldIndex, face->newIndex, 1);
		mesh->dm->getFace(mesh->dm, face->oldIndex, newMF);

		newMF->v1 = face->edges[0]->verts[face->flip[0]]->newIndex;
		newMF->v2 = face->edges[1]->verts[face->flip[1]]->newIndex;
		newMF->v3 = face->edges[2]->verts[face->flip[2]]->newIndex;

		if(face->edges[3]) {
			newMF->v4 = face->edges[3]->verts[face->flip[3]]->newIndex;
		} else {
			newMF->v4 = 0;
		}
	}

	return result;
}

/* returns the other vert in the given edge
 */
static SmoothVert *other_vert(SmoothEdge *edge, SmoothVert *vert)
{
	if(edge->verts[0] == vert) return edge->verts[1];
	else return edge->verts[0];
}

/* returns the other edge in the given face that uses the given vert
 * returns NULL if no other edge in the given face uses the given vert
 * (this should never happen)
 */
static SmoothEdge *other_edge(SmoothFace *face, SmoothVert *vert,
			      SmoothEdge *edge)
{
	int i,j;
	for(i = 0; i < SMOOTHFACE_MAX_EDGES && face->edges[i]; i++) {
		SmoothEdge *tmp_edge = face->edges[i];
		if(tmp_edge == edge) continue;

		for(j = 0; j < SMOOTHEDGE_NUM_VERTS; j++)
			if(tmp_edge->verts[j] == vert) return tmp_edge;
	}

	/* if we get to here, something's wrong (there should always be 2 edges
	* which use the same vert in a face)
	*/
	return NULL;
}

/* returns a face attached to the given edge which is not the given face.
 * returns NULL if no other faces use this edge.
 */
static SmoothFace *other_face(SmoothEdge *edge, SmoothFace *face)
{
	LinkNode *node;

	for(node = edge->faces; node != NULL; node = node->next)
		if(node->link != face) return node->link;

	return NULL;
}

#if 0
/* copies source list to target, overwriting target (target is not freed)
 * nodes in the copy will be in the same order as in source
 */
static void linklist_copy(LinkNode **target, LinkNode *source)
{
	LinkNode *node = NULL;
	*target = NULL;

	for(; source; source = source->next) {
		if(node) {
			node->next = MEM_mallocN(sizeof(*node->next), "nlink_copy");
										node = node->next;
} else {
										node = *target = MEM_mallocN(sizeof(**target), "nlink_copy");
}
										node->link = source->link;
										node->next = NULL;
}
}
#endif

										/* appends source to target if it's not already in target */
										static void linklist_append_unique(LinkNode **target, void *source) 
{
	LinkNode *node;
	LinkNode *prev = NULL;

	/* check if source value is already in the list */
	for(node = *target; node; prev = node, node = node->next)
		if(node->link == source) return;

	node = MEM_mallocN(sizeof(*node), "nlink");
	node->next = NULL;
	node->link = source;

	if(prev) prev->next = node;
	else *target = node;
}

/* appends elements of source which aren't already in target to target */
static void linklist_append_list_unique(LinkNode **target, LinkNode *source)
{
	for(; source; source = source->next)
		linklist_append_unique(target, source->link);
}

#if 0 /* this is no longer used, it should possibly be removed */
/* prepends prepend to list - doesn't copy nodes, just joins the lists */
static void linklist_prepend_linklist(LinkNode **list, LinkNode *prepend)
{
	if(prepend) {
		LinkNode *node = prepend;
		while(node->next) node = node->next;

		node->next = *list;
		*list = prepend;
}
}
#endif

/* returns 1 if the linked list contains the given pointer, 0 otherwise
 */
static int linklist_contains(LinkNode *list, void *ptr)
{
	LinkNode *node;

	for(node = list; node; node = node->next)
		if(node->link == ptr) return 1;

	return 0;
}

/* returns 1 if the first linked list is a subset of the second (comparing
 * pointer values), 0 if not
 */
static int linklist_subset(LinkNode *list1, LinkNode *list2)
{
	for(; list1; list1 = list1->next)
		if(!linklist_contains(list2, list1->link))
			return 0;

	return 1;
}

#if 0
/* empties the linked list
 * frees pointers with freefunc if freefunc is not NULL
 */
static void linklist_empty(LinkNode **list, LinkNodeFreeFP freefunc)
{
	BLI_linklist_free(*list, freefunc);
	*list = NULL;
}
#endif

/* removes the first instance of value from the linked list
 * frees the pointer with freefunc if freefunc is not NULL
 */
static void linklist_remove_first(LinkNode **list, void *value,
				  LinkNodeFreeFP freefunc)
{
	LinkNode *node = *list;
	LinkNode *prev = NULL;

	while(node && node->link != value) {
		prev = node;
		node = node->next;
	}

	if(node) {
		if(prev)
			prev->next = node->next;
		else
			*list = node->next;

		if(freefunc)
			freefunc(node->link);

		MEM_freeN(node);
	}
}

/* removes all elements in source from target */
static void linklist_remove_list(LinkNode **target, LinkNode *source,
				 LinkNodeFreeFP freefunc)
{
	for(; source; source = source->next)
		linklist_remove_first(target, source->link, freefunc);
}

#ifdef EDGESPLIT_DEBUG_0
static void print_ptr(void *ptr)
{
	printf("%p\n", ptr);
}

static void print_edge(void *ptr)
{
	SmoothEdge *edge = ptr;
	printf(" %4d", edge->newIndex);
}

static void print_face(void *ptr)
{
	SmoothFace *face = ptr;
	printf(" %4d", face->newIndex);
}
#endif

typedef struct ReplaceData {
	void *find;
	void *replace;
} ReplaceData;

static void edge_replace_vert(void *ptr, void *userdata)
{
	SmoothEdge *edge = ptr;
	SmoothVert *find = ((ReplaceData *)userdata)->find;
	SmoothVert *replace = ((ReplaceData *)userdata)->replace;
	int i;

#ifdef EDGESPLIT_DEBUG_3
	printf("replacing vert %4d with %4d in edge %4d",
	       find->newIndex, replace->newIndex, edge->newIndex);
	printf(": {%4d, %4d}", edge->verts[0]->newIndex, edge->verts[1]->newIndex);
#endif

	for(i = 0; i < SMOOTHEDGE_NUM_VERTS; i++) {
		if(edge->verts[i] == find) {
			linklist_append_list_unique(&replace->faces, edge->faces);
			linklist_remove_list(&find->faces, edge->faces, NULL);

			edge->verts[i] = replace;
		}
	}

#ifdef EDGESPLIT_DEBUG_3
	printf(" -> {%4d, %4d}\n", edge->verts[0]->newIndex, edge->verts[1]->newIndex);
#endif
}

static void face_replace_vert(void *ptr, void *userdata)
{
	SmoothFace *face = ptr;
	int i;

	for(i = 0; i < SMOOTHFACE_MAX_EDGES && face->edges[i]; i++)
		edge_replace_vert(face->edges[i], userdata);
}

static void face_replace_edge(void *ptr, void *userdata)
{
	SmoothFace *face = ptr;
	SmoothEdge *find = ((ReplaceData *)userdata)->find;
	SmoothEdge *replace = ((ReplaceData *)userdata)->replace;
	int i;

#ifdef EDGESPLIT_DEBUG_3
	printf("replacing edge %4d with %4d in face %4d",
	       find->newIndex, replace->newIndex, face->newIndex);
	if(face->edges[3])
		printf(": {%2d %2d %2d %2d}",
		       face->edges[0]->newIndex, face->edges[1]->newIndex,
	 face->edges[2]->newIndex, face->edges[3]->newIndex);
	else
		printf(": {%2d %2d %2d}",
		       face->edges[0]->newIndex, face->edges[1]->newIndex,
	 face->edges[2]->newIndex);
#endif

	for(i = 0; i < SMOOTHFACE_MAX_EDGES && face->edges[i]; i++) {
		if(face->edges[i] == find) {
			linklist_remove_first(&face->edges[i]->faces, face, NULL);
			BLI_linklist_prepend(&replace->faces, face);
			face->edges[i] = replace;
		}
	}

#ifdef EDGESPLIT_DEBUG_3
	if(face->edges[3])
		printf(" -> {%2d %2d %2d %2d}\n",
		       face->edges[0]->newIndex, face->edges[1]->newIndex,
	 face->edges[2]->newIndex, face->edges[3]->newIndex);
	else
		printf(" -> {%2d %2d %2d}\n",
		       face->edges[0]->newIndex, face->edges[1]->newIndex,
	 face->edges[2]->newIndex);
#endif
}

static int edge_is_loose(SmoothEdge *edge)
{
	return !(edge->faces && edge->faces->next);
}

static int edge_is_sharp(SmoothEdge *edge, int flags,
			 float threshold)
{
#ifdef EDGESPLIT_DEBUG_1
	printf("edge %d: ", edge->newIndex);
#endif
	if(edge->flag & ME_SHARP) {
		/* edge can only be sharp if it has at least 2 faces */
		if(!edge_is_loose(edge)) {
#ifdef EDGESPLIT_DEBUG_1
			printf("sharp\n");
#endif
			return 1;
		} else {
			/* edge is loose, so it can't be sharp */
			edge->flag &= ~ME_SHARP;
		}
	}

#ifdef EDGESPLIT_DEBUG_1
	printf("not sharp\n");
#endif
	return 0;
}

/* finds another sharp edge which uses vert, by traversing faces around the
 * vert until it does one of the following:
 * - hits a loose edge (the edge is returned)
 * - hits a sharp edge (the edge is returned)
 * - returns to the start edge (NULL is returned)
 */
static SmoothEdge *find_other_sharp_edge(SmoothVert *vert, SmoothEdge *edge,
					 LinkNode **visited_faces, float threshold, int flags)
{
	SmoothFace *face = NULL;
	SmoothEdge *edge2 = NULL;
	/* holds the edges we've seen so we can avoid looping indefinitely */
	LinkNode *visited_edges = NULL;
#ifdef EDGESPLIT_DEBUG_1
	printf("=== START === find_other_sharp_edge(edge = %4d, vert = %4d)\n",
	       edge->newIndex, vert->newIndex);
#endif

	/* get a face on which to start */
	if(edge->faces) face = edge->faces->link;
	else return NULL;

	/* record this edge as visited */
	BLI_linklist_prepend(&visited_edges, edge);

	/* get the next edge */
	edge2 = other_edge(face, vert, edge);

	/* record this face as visited */
	if(visited_faces)
		BLI_linklist_prepend(visited_faces, face);

	/* search until we hit a loose edge or a sharp edge or an edge we've
	* seen before
	*/
	while(face && !edge_is_sharp(edge2, flags, threshold)
		     && !linklist_contains(visited_edges, edge2)) {
#ifdef EDGESPLIT_DEBUG_3
		printf("current face %4d; current edge %4d\n", face->newIndex,
		       edge2->newIndex);
#endif
		/* get the next face */
		face = other_face(edge2, face);

		/* if face == NULL, edge2 is a loose edge */
		if(face) {
			/* record this face as visited */
			if(visited_faces)
				BLI_linklist_prepend(visited_faces, face);

			/* record this edge as visited */
			BLI_linklist_prepend(&visited_edges, edge2);

			/* get the next edge */
			edge2 = other_edge(face, vert, edge2);
#ifdef EDGESPLIT_DEBUG_3
			printf("next face %4d; next edge %4d\n",
			       face->newIndex, edge2->newIndex);
		} else {
			printf("loose edge: %4d\n", edge2->newIndex);
#endif
		}
		     }

		     /* either we came back to the start edge or we found a sharp/loose edge */
		     if(linklist_contains(visited_edges, edge2))
			     /* we came back to the start edge */
			     edge2 = NULL;

		     BLI_linklist_free(visited_edges, NULL);

#ifdef EDGESPLIT_DEBUG_1
		     printf("=== END === find_other_sharp_edge(edge = %4d, vert = %4d), "
				     "returning edge %d\n",
	 edge->newIndex, vert->newIndex, edge2 ? edge2->newIndex : -1);
#endif
		     return edge2;
}

static void split_single_vert(SmoothVert *vert, SmoothFace *face,
			      SmoothMesh *mesh)
{
	SmoothVert *copy_vert;
	ReplaceData repdata;

	copy_vert = smoothvert_copy(vert, mesh);

	repdata.find = vert;
	repdata.replace = copy_vert;
	face_replace_vert(face, &repdata);
}

typedef struct PropagateEdge {
	struct PropagateEdge *next, *prev;
	SmoothEdge *edge;
	SmoothVert *vert;
} PropagateEdge;

static void push_propagate_stack(SmoothEdge *edge, SmoothVert *vert, SmoothMesh *mesh)
{
	PropagateEdge *pedge = mesh->reusestack.first;

	if(pedge) {
		BLI_remlink(&mesh->reusestack, pedge);
	}
	else {
		if(!mesh->arena) {
			mesh->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
			BLI_memarena_use_calloc(mesh->arena);
		}

		pedge = BLI_memarena_alloc(mesh->arena, sizeof(PropagateEdge));
	}

	pedge->edge = edge;
	pedge->vert = vert;
	BLI_addhead(&mesh->propagatestack, pedge);
}

static void pop_propagate_stack(SmoothEdge **edge, SmoothVert **vert, SmoothMesh *mesh)
{
	PropagateEdge *pedge = mesh->propagatestack.first;

	if(pedge) {
		*edge = pedge->edge;
		*vert = pedge->vert;
		BLI_remlink(&mesh->propagatestack, pedge);
		BLI_addhead(&mesh->reusestack, pedge);
	}
	else {
		*edge = NULL;
		*vert = NULL;
	}
}

static void split_edge(SmoothEdge *edge, SmoothVert *vert, SmoothMesh *mesh);

static void propagate_split(SmoothEdge *edge, SmoothVert *vert,
			    SmoothMesh *mesh)
{
	SmoothEdge *edge2;
	LinkNode *visited_faces = NULL;
#ifdef EDGESPLIT_DEBUG_1
	printf("=== START === propagate_split(edge = %4d, vert = %4d)\n",
	       edge->newIndex, vert->newIndex);
#endif

	edge2 = find_other_sharp_edge(vert, edge, &visited_faces,
				      mesh->threshold, mesh->flags);

	if(!edge2) {
		/* didn't find a sharp or loose edge, so we've hit a dead end */
	} else if(!edge_is_loose(edge2)) {
		/* edge2 is not loose, so it must be sharp */
		if(edge_is_loose(edge)) {
			/* edge is loose, so we can split edge2 at this vert */
			split_edge(edge2, vert, mesh);
		} else if(edge_is_sharp(edge, mesh->flags, mesh->threshold)) {
			/* both edges are sharp, so we can split the pair at vert */
			split_edge(edge, vert, mesh);
		} else {
			/* edge is not sharp, so try to split edge2 at its other vert */
			split_edge(edge2, other_vert(edge2, vert), mesh);
		}
	} else { /* edge2 is loose */
		if(edge_is_loose(edge)) {
			SmoothVert *vert2;
			ReplaceData repdata;

			/* can't split edge, what should we do with vert? */
			if(linklist_subset(vert->faces, visited_faces)) {
				/* vert has only one fan of faces attached; don't split it */
			} else {
				/* vert has more than one fan of faces attached; split it */
				vert2 = smoothvert_copy(vert, mesh);

				/* replace vert with its copy in visited_faces */
				repdata.find = vert;
				repdata.replace = vert2;
				BLI_linklist_apply(visited_faces, face_replace_vert, &repdata);
			}
		} else {
			/* edge is not loose, so it must be sharp; split it */
			split_edge(edge, vert, mesh);
		}
	}

	BLI_linklist_free(visited_faces, NULL);
#ifdef EDGESPLIT_DEBUG_1
	printf("=== END === propagate_split(edge = %4d, vert = %4d)\n",
	       edge->newIndex, vert->newIndex);
#endif
}

static void split_edge(SmoothEdge *edge, SmoothVert *vert, SmoothMesh *mesh)
{
	SmoothEdge *edge2;
	SmoothVert *vert2;
	ReplaceData repdata;
	/* the list of faces traversed while looking for a sharp edge */
	LinkNode *visited_faces = NULL;
#ifdef EDGESPLIT_DEBUG_1
	printf("=== START === split_edge(edge = %4d, vert = %4d)\n",
	       edge->newIndex, vert->newIndex);
#endif

	edge2 = find_other_sharp_edge(vert, edge, &visited_faces,
				      mesh->threshold, mesh->flags);

	if(!edge2) {
		/* didn't find a sharp or loose edge, so try the other vert */
		vert2 = other_vert(edge, vert);
		push_propagate_stack(edge, vert2, mesh);
	} else if(!edge_is_loose(edge2)) {
		/* edge2 is not loose, so it must be sharp */
		SmoothEdge *copy_edge = smoothedge_copy(edge, mesh);
		SmoothEdge *copy_edge2 = smoothedge_copy(edge2, mesh);
		SmoothVert *vert2;

		/* replace edge with its copy in visited_faces */
		repdata.find = edge;
		repdata.replace = copy_edge;
		BLI_linklist_apply(visited_faces, face_replace_edge, &repdata);

		/* replace edge2 with its copy in visited_faces */
		repdata.find = edge2;
		repdata.replace = copy_edge2;
		BLI_linklist_apply(visited_faces, face_replace_edge, &repdata);

		vert2 = smoothvert_copy(vert, mesh);

		/* replace vert with its copy in visited_faces (must be done after
		* edge replacement so edges have correct vertices)
		*/
		repdata.find = vert;
		repdata.replace = vert2;
		BLI_linklist_apply(visited_faces, face_replace_vert, &repdata);

		/* all copying and replacing is done; the mesh should be consistent.
		* now propagate the split to the vertices at either end
		*/
		push_propagate_stack(copy_edge, other_vert(copy_edge, vert2), mesh);
		push_propagate_stack(copy_edge2, other_vert(copy_edge2, vert2), mesh);

		if(smoothedge_has_vert(edge, vert))
			push_propagate_stack(edge, vert, mesh);
	} else {
		/* edge2 is loose */
		SmoothEdge *copy_edge = smoothedge_copy(edge, mesh);
		SmoothVert *vert2;

		/* replace edge with its copy in visited_faces */
		repdata.find = edge;
		repdata.replace = copy_edge;
		BLI_linklist_apply(visited_faces, face_replace_edge, &repdata);

		vert2 = smoothvert_copy(vert, mesh);

		/* replace vert with its copy in visited_faces (must be done after
		* edge replacement so edges have correct vertices)
		*/
		repdata.find = vert;
		repdata.replace = vert2;
		BLI_linklist_apply(visited_faces, face_replace_vert, &repdata);

		/* copying and replacing is done; the mesh should be consistent.
		* now propagate the split to the vertex at the other end
		*/
		push_propagate_stack(copy_edge, other_vert(copy_edge, vert2), mesh);

		if(smoothedge_has_vert(edge, vert))
			push_propagate_stack(edge, vert, mesh);
	}

	BLI_linklist_free(visited_faces, NULL);
#ifdef EDGESPLIT_DEBUG_1
	printf("=== END === split_edge(edge = %4d, vert = %4d)\n",
	       edge->newIndex, vert->newIndex);
#endif
}

static void tag_and_count_extra_edges(SmoothMesh *mesh, float split_angle,
				      int flags, int *extra_edges)
{
	/* if normal1 dot normal2 < threshold, angle is greater, so split */
	/* FIXME not sure if this always works */
	/* 0.00001 added for floating-point rounding */
	float threshold = cos((split_angle + 0.00001) * M_PI / 180.0);
	int i;

	*extra_edges = 0;

	/* loop through edges, counting potential new ones */
	for(i = 0; i < mesh->num_edges; i++) {
		SmoothEdge *edge = &mesh->edges[i];
		int sharp = 0;

		/* treat all non-manifold edges (3 or more faces) as sharp */
		if(edge->faces && edge->faces->next && edge->faces->next->next) {
			LinkNode *node;

			/* this edge is sharp */
			sharp = 1;

			/* add an extra edge for every face beyond the first */
			*extra_edges += 2;
			for(node = edge->faces->next->next->next; node; node = node->next)
				(*extra_edges)++;
		} else if((flags & (MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG))
					 && !edge_is_loose(edge)) {
			/* (the edge can only be sharp if we're checking angle or flag,
			* and it has at least 2 faces) */

						 /* if we're checking the sharp flag and it's set, good */
						 if((flags & MOD_EDGESPLIT_FROMFLAG) && (edge->flag & ME_SHARP)) {
							 /* this edge is sharp */
							 sharp = 1;

							 (*extra_edges)++;
						 } else if(flags & MOD_EDGESPLIT_FROMANGLE) {
							 /* we know the edge has 2 faces, so check the angle */
							 SmoothFace *face1 = edge->faces->link;
							 SmoothFace *face2 = edge->faces->next->link;
							 float edge_angle_cos = dot_v3v3(face1->normal,
									 face2->normal);

							 if(edge_angle_cos < threshold) {
								 /* this edge is sharp */
								 sharp = 1;

								 (*extra_edges)++;
							 }
						 }
					 }

					 /* set/clear sharp flag appropriately */
					 if(sharp) edge->flag |= ME_SHARP;
					 else edge->flag &= ~ME_SHARP;
	}
}

static void split_sharp_edges(SmoothMesh *mesh, float split_angle, int flags)
{
	SmoothVert *vert;
	int i;
	/* if normal1 dot normal2 < threshold, angle is greater, so split */
	/* FIXME not sure if this always works */
	/* 0.00001 added for floating-point rounding */
	mesh->threshold = cos((split_angle + 0.00001) * M_PI / 180.0);
	mesh->flags = flags;

	/* loop through edges, splitting sharp ones */
	/* can't use an iterator here, because we'll be adding edges */
	for(i = 0; i < mesh->num_edges; i++) {
		SmoothEdge *edge = &mesh->edges[i];

		if(edge_is_sharp(edge, flags, mesh->threshold)) {
			split_edge(edge, edge->verts[0], mesh);

			do {
				pop_propagate_stack(&edge, &vert, mesh);
				if(edge && smoothedge_has_vert(edge, vert))
					propagate_split(edge, vert, mesh);
			} while(edge);
		}
	}
}

static int count_bridge_verts(SmoothMesh *mesh)
{
	int i, j, count = 0;

	for(i = 0; i < mesh->num_faces; i++) {
		SmoothFace *face = &mesh->faces[i];

		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			SmoothEdge *edge = face->edges[j];
			SmoothEdge *next_edge;
			SmoothVert *vert = edge->verts[1 - face->flip[j]];
			int next = (j + 1) % SMOOTHFACE_MAX_EDGES;

			/* wrap next around if at last edge */
			if(!face->edges[next]) next = 0;

			next_edge = face->edges[next];

			/* if there are other faces sharing this vertex but not
			* these edges, the vertex will be split, so count it
			*/
			/* vert has to have at least one face (this one), so faces != 0 */
			if(!edge->faces->next && !next_edge->faces->next
						 && vert->faces->next) {
				count++;
						 }
		}
	}

	/* each bridge vert will be counted once per face that uses it,
	* so count is too high, but it's ok for now
	*/
	return count;
}

static void split_bridge_verts(SmoothMesh *mesh)
{
	int i,j;

	for(i = 0; i < mesh->num_faces; i++) {
		SmoothFace *face = &mesh->faces[i];

		for(j = 0; j < SMOOTHFACE_MAX_EDGES && face->edges[j]; j++) {
			SmoothEdge *edge = face->edges[j];
			SmoothEdge *next_edge;
			SmoothVert *vert = edge->verts[1 - face->flip[j]];
			int next = (j + 1) % SMOOTHFACE_MAX_EDGES;

			/* wrap next around if at last edge */
			if(!face->edges[next]) next = 0;

			next_edge = face->edges[next];

			/* if there are other faces sharing this vertex but not
			* these edges, split the vertex
			*/
			/* vert has to have at least one face (this one), so faces != 0 */
			if(!edge->faces->next && !next_edge->faces->next
						 && vert->faces->next)
				/* FIXME this needs to find all faces that share edges with
				* this one and split off together
				*/
				split_single_vert(vert, face, mesh);
		}
	}
}

static DerivedMesh *edgesplitModifier_do(EdgeSplitModifierData *emd,
					 Object *ob, DerivedMesh *dm)
{
	SmoothMesh *mesh;
	DerivedMesh *result;
	int max_verts, max_edges;

	if(!(emd->flags & (MOD_EDGESPLIT_FROMANGLE | MOD_EDGESPLIT_FROMFLAG)))
		return dm;

	/* 1. make smoothmesh with initial number of elements */
	mesh = smoothmesh_from_derivedmesh(dm);

	/* 2. count max number of elements to add */
	tag_and_count_extra_edges(mesh, emd->split_angle, emd->flags, &max_edges);
	max_verts = max_edges * 2 + mesh->max_verts;
	max_verts += count_bridge_verts(mesh);
	max_edges += mesh->max_edges;

	/* 3. reallocate smoothmesh arrays & copy elements across */
	/* 4. remap copied elements' pointers to point into the new arrays */
	smoothmesh_resize_verts(mesh, max_verts);
	smoothmesh_resize_edges(mesh, max_edges);

#ifdef EDGESPLIT_DEBUG_1
	printf("********** Pre-split **********\n");
	smoothmesh_print(mesh);
#endif

	split_sharp_edges(mesh, emd->split_angle, emd->flags);
#ifdef EDGESPLIT_DEBUG_1
	printf("********** Post-edge-split **********\n");
	smoothmesh_print(mesh);
#endif

	split_bridge_verts(mesh);

#ifdef EDGESPLIT_DEBUG_1
	printf("********** Post-vert-split **********\n");
	smoothmesh_print(mesh);
#endif

#ifdef EDGESPLIT_DEBUG_0
	printf("Edgesplit: Estimated %d verts & %d edges, "
			"found %d verts & %d edges\n", max_verts, max_edges,
   mesh->num_verts, mesh->num_edges);
#endif

	result = CDDM_from_smoothmesh(mesh);
	smoothmesh_free(mesh);

	return result;
}

static DerivedMesh *edgesplitModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	EdgeSplitModifierData *emd = (EdgeSplitModifierData*) md;

	result = edgesplitModifier_do(emd, ob, derivedData);

	if(result != derivedData)
		CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *edgesplitModifier_applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return edgesplitModifier_applyModifier(md, ob, derivedData, 0, 1);
}

/* Bevel */

static void bevelModifier_initData(ModifierData *md)
{
	BevelModifierData *bmd = (BevelModifierData*) md;

	bmd->value = 0.1f;
	bmd->res = 1;
	bmd->flags = 0;
	bmd->val_flags = 0;
	bmd->lim_flags = 0;
	bmd->e_flags = 0;
	bmd->bevel_angle = 30;
	bmd->defgrp_name[0] = '\0';
}

static void bevelModifier_copyData(ModifierData *md, ModifierData *target)
{
	BevelModifierData *bmd = (BevelModifierData*) md;
	BevelModifierData *tbmd = (BevelModifierData*) target;

	tbmd->value = bmd->value;
	tbmd->res = bmd->res;
	tbmd->flags = bmd->flags;
	tbmd->val_flags = bmd->val_flags;
	tbmd->lim_flags = bmd->lim_flags;
	tbmd->e_flags = bmd->e_flags;
	tbmd->bevel_angle = bmd->bevel_angle;
	strncpy(tbmd->defgrp_name, bmd->defgrp_name, 32);
}

static CustomDataMask bevelModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	BevelModifierData *bmd = (BevelModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(bmd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static DerivedMesh *bevelModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	BME_Mesh *bm;

	/*bDeformGroup *def;*/
	int /*i,*/ options, defgrp_index = -1;
	BevelModifierData *bmd = (BevelModifierData*) md;

	options = bmd->flags|bmd->val_flags|bmd->lim_flags|bmd->e_flags;

	//~ if ((options & BME_BEVEL_VWEIGHT) && bmd->defgrp_name[0]) {
		//~ for (i = 0, def = ob->defbase.first; def; def = def->next, i++) {
			//~ if (!strcmp(def->name, bmd->defgrp_name)) {
				//~ defgrp_index = i;
				//~ break;
			//~ }
		//~ }
		//~ if (defgrp_index < 0) {
			//~ options &= ~BME_BEVEL_VWEIGHT;
		//~ }
	//~ }

	bm = BME_derivedmesh_to_bmesh(derivedData);
	BME_bevel(bm,bmd->value,bmd->res,options,defgrp_index,bmd->bevel_angle,NULL);
	result = BME_bmesh_to_derivedmesh(bm,derivedData);
	BME_free_mesh(bm);

	CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *bevelModifier_applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return bevelModifier_applyModifier(md, ob, derivedData, 0, 1);
}

/* Displace */

static void displaceModifier_initData(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	dmd->texture = NULL;
	dmd->strength = 1;
	dmd->direction = MOD_DISP_DIR_NOR;
	dmd->midlevel = 0.5;
}

static void displaceModifier_copyData(ModifierData *md, ModifierData *target)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;
	DisplaceModifierData *tdmd = (DisplaceModifierData*) target;

	tdmd->texture = dmd->texture;
	tdmd->strength = dmd->strength;
	tdmd->direction = dmd->direction;
	strncpy(tdmd->defgrp_name, dmd->defgrp_name, 32);
	tdmd->midlevel = dmd->midlevel;
	tdmd->texmapping = dmd->texmapping;
	tdmd->map_object = dmd->map_object;
	strncpy(tdmd->uvlayer_name, dmd->uvlayer_name, 32);
}

static CustomDataMask displaceModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(dmd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	/* ask for UV coordinates if we need them */
	if(dmd->texmapping == MOD_DISP_MAP_UV) dataMask |= (1 << CD_MTFACE);

	return dataMask;
}

static int displaceModifier_dependsOnTime(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;

	if(dmd->texture)
	{
		return BKE_texture_dependsOnTime(dmd->texture);
	}
	else
	{
		return 0;
	}
}

static void displaceModifier_foreachObjectLink(ModifierData *md, Object *ob,
					       ObjectWalkFunc walk, void *userData)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	walk(userData, ob, &dmd->map_object);
}

static void displaceModifier_foreachIDLink(ModifierData *md, Object *ob,
					   IDWalkFunc walk, void *userData)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	walk(userData, ob, (ID **)&dmd->texture);

	displaceModifier_foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static int displaceModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	return !dmd->texture;
}

static void displaceModifier_updateDepgraph(
					    ModifierData *md, DagForest *forest, Scene *scene,
	 Object *ob, DagNode *obNode)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	if(dmd->map_object) {
		DagNode *curNode = dag_get_node(forest, dmd->map_object);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Displace Modifier");
	}
}

static void validate_layer_name(const CustomData *data, int type, char *name, char *outname)
{
	int index = -1;

	/* if a layer name was given, try to find that layer */
	if(name[0])
		index = CustomData_get_named_layer_index(data, CD_MTFACE, name);

	if(index < 0) {
		/* either no layer was specified, or the layer we want has been
		* deleted, so assign the active layer to name
		*/
		index = CustomData_get_active_layer_index(data, CD_MTFACE);
		strcpy(outname, data->layers[index].name);
	}
	else
		strcpy(outname, name);
}

static void get_texture_coords(DisplaceModifierData *dmd, Object *ob,
			       DerivedMesh *dm,
	  float (*co)[3], float (*texco)[3],
		  int numVerts)
{
	int i;
	int texmapping = dmd->texmapping;
	float mapob_imat[4][4];

	if(texmapping == MOD_DISP_MAP_OBJECT) {
		if(dmd->map_object)
			invert_m4_m4(mapob_imat, dmd->map_object->obmat);
		else /* if there is no map object, default to local */
			texmapping = MOD_DISP_MAP_LOCAL;
	}

	/* UVs need special handling, since they come from faces */
	if(texmapping == MOD_DISP_MAP_UV) {
		if(CustomData_has_layer(&dm->faceData, CD_MTFACE)) {
			MFace *mface = dm->getFaceArray(dm);
			MFace *mf;
			char *done = MEM_callocN(sizeof(*done) * numVerts,
					"get_texture_coords done");
			int numFaces = dm->getNumFaces(dm);
			char uvname[32];
			MTFace *tf;

			validate_layer_name(&dm->faceData, CD_MTFACE, dmd->uvlayer_name, uvname);
			tf = CustomData_get_layer_named(&dm->faceData, CD_MTFACE, uvname);

			/* verts are given the UV from the first face that uses them */
			for(i = 0, mf = mface; i < numFaces; ++i, ++mf, ++tf) {
				if(!done[mf->v1]) {
					texco[mf->v1][0] = tf->uv[0][0];
					texco[mf->v1][1] = tf->uv[0][1];
					texco[mf->v1][2] = 0;
					done[mf->v1] = 1;
				}
				if(!done[mf->v2]) {
					texco[mf->v2][0] = tf->uv[1][0];
					texco[mf->v2][1] = tf->uv[1][1];
					texco[mf->v2][2] = 0;
					done[mf->v2] = 1;
				}
				if(!done[mf->v3]) {
					texco[mf->v3][0] = tf->uv[2][0];
					texco[mf->v3][1] = tf->uv[2][1];
					texco[mf->v3][2] = 0;
					done[mf->v3] = 1;
				}
				if(!done[mf->v4]) {
					texco[mf->v4][0] = tf->uv[3][0];
					texco[mf->v4][1] = tf->uv[3][1];
					texco[mf->v4][2] = 0;
					done[mf->v4] = 1;
				}
			}

			/* remap UVs from [0, 1] to [-1, 1] */
			for(i = 0; i < numVerts; ++i) {
				texco[i][0] = texco[i][0] * 2 - 1;
				texco[i][1] = texco[i][1] * 2 - 1;
			}

			MEM_freeN(done);
			return;
		} else /* if there are no UVs, default to local */
			texmapping = MOD_DISP_MAP_LOCAL;
	}

	for(i = 0; i < numVerts; ++i, ++co, ++texco) {
		switch(texmapping) {
			case MOD_DISP_MAP_LOCAL:
				VECCOPY(*texco, *co);
				break;
			case MOD_DISP_MAP_GLOBAL:
				VECCOPY(*texco, *co);
				mul_m4_v3(ob->obmat, *texco);
				break;
			case MOD_DISP_MAP_OBJECT:
				VECCOPY(*texco, *co);
				mul_m4_v3(ob->obmat, *texco);
				mul_m4_v3(mapob_imat, *texco);
				break;
		}
	}
}

static void get_texture_value(Tex *texture, float *tex_co, TexResult *texres)
{
	int result_type;

	result_type = multitex_ext(texture, tex_co, NULL,
				   NULL, 1, texres);

	/* if the texture gave an RGB value, we assume it didn't give a valid
	* intensity, so calculate one (formula from do_material_tex).
	* if the texture didn't give an RGB value, copy the intensity across
	*/
	if(result_type & TEX_RGB)
		texres->tin = (0.35f * texres->tr + 0.45f * texres->tg
				+ 0.2f * texres->tb);
	else
		texres->tr = texres->tg = texres->tb = texres->tin;
}

/* dm must be a CDDerivedMesh */
static void displaceModifier_do(
				DisplaceModifierData *dmd, Object *ob,
    DerivedMesh *dm, float (*vertexCos)[3], int numVerts)
{
	int i;
	MVert *mvert;
	MDeformVert *dvert = NULL;
	int defgrp_index;
	float (*tex_co)[3];

	if(!dmd->texture) return;

	defgrp_index = -1;

	if(dmd->defgrp_name[0]) {
		bDeformGroup *def;
		for(i = 0, def = ob->defbase.first; def; def = def->next, i++) {
			if(!strcmp(def->name, dmd->defgrp_name)) {
				defgrp_index = i;
				break;
			}
		}
	}

	mvert = CDDM_get_verts(dm);
	if(defgrp_index >= 0)
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

	tex_co = MEM_callocN(sizeof(*tex_co) * numVerts,
			     "displaceModifier_do tex_co");
	get_texture_coords(dmd, ob, dm, vertexCos, tex_co, numVerts);

	for(i = 0; i < numVerts; ++i) {
		TexResult texres;
		float delta = 0, strength = dmd->strength;
		MDeformWeight *def_weight = NULL;

		if(dvert) {
			int j;
			for(j = 0; j < dvert[i].totweight; ++j) {
				if(dvert[i].dw[j].def_nr == defgrp_index) {
					def_weight = &dvert[i].dw[j];
					break;
				}
			}
			if(!def_weight) continue;
		}

		texres.nor = NULL;
		get_texture_value(dmd->texture, tex_co[i], &texres);

		delta = texres.tin - dmd->midlevel;

		if(def_weight) strength *= def_weight->weight;

		delta *= strength;

		switch(dmd->direction) {
			case MOD_DISP_DIR_X:
				vertexCos[i][0] += delta;
				break;
			case MOD_DISP_DIR_Y:
				vertexCos[i][1] += delta;
				break;
			case MOD_DISP_DIR_Z:
				vertexCos[i][2] += delta;
				break;
			case MOD_DISP_DIR_RGB_XYZ:
				vertexCos[i][0] += (texres.tr - dmd->midlevel) * strength;
				vertexCos[i][1] += (texres.tg - dmd->midlevel) * strength;
				vertexCos[i][2] += (texres.tb - dmd->midlevel) * strength;
				break;
			case MOD_DISP_DIR_NOR:
				vertexCos[i][0] += delta * mvert[i].no[0] / 32767.0f;
				vertexCos[i][1] += delta * mvert[i].no[1] / 32767.0f;
				vertexCos[i][2] += delta * mvert[i].no[2] / 32767.0f;
				break;
		}
	}

	MEM_freeN(tex_co);
}

static void displaceModifier_deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
      float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm= get_cddm(md->scene, ob, NULL, derivedData, vertexCos);

	displaceModifier_do((DisplaceModifierData *)md, ob, dm,
			     vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void displaceModifier_deformVertsEM(
					   ModifierData *md, Object *ob, EditMesh *editData,
	DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm= get_cddm(md->scene, ob, editData, derivedData, vertexCos);

	displaceModifier_do((DisplaceModifierData *)md, ob, dm,
			     vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

/* UVProject */
/* UV Project modifier: Generates UVs projected from an object
*/

static void uvprojectModifier_initData(ModifierData *md)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;
	int i;

	for(i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; ++i)
		umd->projectors[i] = NULL;
	umd->image = NULL;
	umd->flags = 0;
	umd->num_projectors = 1;
	umd->aspectx = umd->aspecty = 1.0f;
}

static void uvprojectModifier_copyData(ModifierData *md, ModifierData *target)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;
	UVProjectModifierData *tumd = (UVProjectModifierData*) target;
	int i;

	for(i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; ++i)
		tumd->projectors[i] = umd->projectors[i];
	tumd->image = umd->image;
	tumd->flags = umd->flags;
	tumd->num_projectors = umd->num_projectors;
	tumd->aspectx = umd->aspectx;
	tumd->aspecty = umd->aspecty;
}

static CustomDataMask uvprojectModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	CustomDataMask dataMask = 0;

	/* ask for UV coordinates */
	dataMask |= (1 << CD_MTFACE);

	return dataMask;
}

static void uvprojectModifier_foreachObjectLink(ModifierData *md, Object *ob,
		ObjectWalkFunc walk, void *userData)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;
	int i;

	for(i = 0; i < MOD_UVPROJECT_MAXPROJECTORS; ++i)
		walk(userData, ob, &umd->projectors[i]);
}

static void uvprojectModifier_foreachIDLink(ModifierData *md, Object *ob,
					    IDWalkFunc walk, void *userData)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;

	walk(userData, ob, (ID **)&umd->image);

	uvprojectModifier_foreachObjectLink(md, ob, (ObjectWalkFunc)walk,
					    userData);
}

static void uvprojectModifier_updateDepgraph(ModifierData *md,
					     DagForest *forest, Scene *scene, Object *ob, DagNode *obNode)
{
	UVProjectModifierData *umd = (UVProjectModifierData*) md;
	int i;

	for(i = 0; i < umd->num_projectors; ++i) {
		if(umd->projectors[i]) {
			DagNode *curNode = dag_get_node(forest, umd->projectors[i]);

			dag_add_relation(forest, curNode, obNode,
					 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "UV Project Modifier");
		}
	}
}

typedef struct Projector {
	Object *ob;				/* object this projector is derived from */
	float projmat[4][4];	/* projection matrix */ 
	float normal[3];		/* projector normal in world space */
} Projector;

static DerivedMesh *uvprojectModifier_do(UVProjectModifierData *umd,
					 Object *ob, DerivedMesh *dm)
{
	float (*coords)[3], (*co)[3];
	MTFace *tface;
	int i, numVerts, numFaces;
	Image *image = umd->image;
	MFace *mface, *mf;
	int override_image = ((umd->flags & MOD_UVPROJECT_OVERRIDEIMAGE) != 0);
	Projector projectors[MOD_UVPROJECT_MAXPROJECTORS];
	int num_projectors = 0;
	float aspect;
	char uvname[32];
	
	if(umd->aspecty != 0) aspect = umd->aspectx / umd->aspecty;
	else aspect = 1.0f;

	for(i = 0; i < umd->num_projectors; ++i)
		if(umd->projectors[i])
			projectors[num_projectors++].ob = umd->projectors[i];

	if(num_projectors == 0) return dm;

	/* make sure there are UV layers available */

	if(!CustomData_has_layer(&dm->faceData, CD_MTFACE)) return dm;

	/* make sure we're using an existing layer */
	validate_layer_name(&dm->faceData, CD_MTFACE, umd->uvlayer_name, uvname);

	/* make sure we are not modifying the original UV layer */
	tface = CustomData_duplicate_referenced_layer_named(&dm->faceData,
			CD_MTFACE, uvname);

	numVerts = dm->getNumVerts(dm);

	coords = MEM_callocN(sizeof(*coords) * numVerts,
			     "uvprojectModifier_do coords");
	dm->getVertCos(dm, coords);

	/* convert coords to world space */
	for(i = 0, co = coords; i < numVerts; ++i, ++co)
		mul_m4_v3(ob->obmat, *co);

	/* calculate a projection matrix and normal for each projector */
	for(i = 0; i < num_projectors; ++i) {
		float tmpmat[4][4];
		float offsetmat[4][4];
		Camera *cam = NULL;
		/* calculate projection matrix */
		invert_m4_m4(projectors[i].projmat, projectors[i].ob->obmat);

		if(projectors[i].ob->type == OB_CAMERA) {
			cam = (Camera *)projectors[i].ob->data;
			if(cam->type == CAM_PERSP) {
				float perspmat[4][4];
				float xmax; 
				float xmin;
				float ymax;
				float ymin;
				float pixsize = cam->clipsta * 32.0 / cam->lens;

				if(aspect > 1.0f) {
					xmax = 0.5f * pixsize;
					ymax = xmax / aspect;
				} else {
					ymax = 0.5f * pixsize;
					xmax = ymax * aspect; 
				}
				xmin = -xmax;
				ymin = -ymax;

				perspective_m4( perspmat,xmin, xmax, ymin, ymax, cam->clipsta, cam->clipend);
				mul_m4_m4m4(tmpmat, projectors[i].projmat, perspmat);
			} else if(cam->type == CAM_ORTHO) {
				float orthomat[4][4];
				float xmax; 
				float xmin;
				float ymax;
				float ymin;

				if(aspect > 1.0f) {
					xmax = 0.5f * cam->ortho_scale; 
					ymax = xmax / aspect;
				} else {
					ymax = 0.5f * cam->ortho_scale;
					xmax = ymax * aspect; 
				}
				xmin = -xmax;
				ymin = -ymax;

				orthographic_m4( orthomat,xmin, xmax, ymin, ymax, cam->clipsta, cam->clipend);
				mul_m4_m4m4(tmpmat, projectors[i].projmat, orthomat);
			}
		} else {
			copy_m4_m4(tmpmat, projectors[i].projmat);
		}

		unit_m4(offsetmat);
		mul_mat3_m4_fl(offsetmat, 0.5);
		offsetmat[3][0] = offsetmat[3][1] = offsetmat[3][2] = 0.5;
		
		if (cam) {
			if (umd->aspectx == umd->aspecty) { 
				offsetmat[3][0] -= cam->shiftx;
				offsetmat[3][1] -= cam->shifty;
			} else if (umd->aspectx < umd->aspecty)  {
				offsetmat[3][0] -=(cam->shiftx * umd->aspecty/umd->aspectx);
				offsetmat[3][1] -= cam->shifty;
			} else {
				offsetmat[3][0] -= cam->shiftx;
				offsetmat[3][1] -=(cam->shifty * umd->aspectx/umd->aspecty);
			}
		}
		
		mul_m4_m4m4(projectors[i].projmat, tmpmat, offsetmat);

		/* calculate worldspace projector normal (for best projector test) */
		projectors[i].normal[0] = 0;
		projectors[i].normal[1] = 0;
		projectors[i].normal[2] = 1;
		mul_mat3_m4_v3(projectors[i].ob->obmat, projectors[i].normal);
	}

	/* if only one projector, project coords to UVs */
	if(num_projectors == 1)
		for(i = 0, co = coords; i < numVerts; ++i, ++co)
			mul_project_m4_v4(projectors[0].projmat, *co);

	mface = dm->getFaceArray(dm);
	numFaces = dm->getNumFaces(dm);

	/* apply coords as UVs, and apply image if tfaces are new */
	for(i = 0, mf = mface; i < numFaces; ++i, ++mf, ++tface) {
		if(override_image || !image || tface->tpage == image) {
			if(num_projectors == 1) {
				/* apply transformed coords as UVs */
				tface->uv[0][0] = coords[mf->v1][0];
				tface->uv[0][1] = coords[mf->v1][1];
				tface->uv[1][0] = coords[mf->v2][0];
				tface->uv[1][1] = coords[mf->v2][1];
				tface->uv[2][0] = coords[mf->v3][0];
				tface->uv[2][1] = coords[mf->v3][1];
				if(mf->v4) {
					tface->uv[3][0] = coords[mf->v4][0];
					tface->uv[3][1] = coords[mf->v4][1];
				}
			} else {
				/* multiple projectors, select the closest to face normal
				* direction
				*/
				float co1[3], co2[3], co3[3], co4[3];
				float face_no[3];
				int j;
				Projector *best_projector;
				float best_dot;

				VECCOPY(co1, coords[mf->v1]);
				VECCOPY(co2, coords[mf->v2]);
				VECCOPY(co3, coords[mf->v3]);

				/* get the untransformed face normal */
				if(mf->v4) {
					VECCOPY(co4, coords[mf->v4]);
					normal_quad_v3( face_no,co1, co2, co3, co4);
				} else { 
					normal_tri_v3( face_no,co1, co2, co3);
				}

				/* find the projector which the face points at most directly
				* (projector normal with largest dot product is best)
				*/
				best_dot = dot_v3v3(projectors[0].normal, face_no);
				best_projector = &projectors[0];

				for(j = 1; j < num_projectors; ++j) {
					float tmp_dot = dot_v3v3(projectors[j].normal,
							face_no);
					if(tmp_dot > best_dot) {
						best_dot = tmp_dot;
						best_projector = &projectors[j];
					}
				}

				mul_project_m4_v4(best_projector->projmat, co1);
				mul_project_m4_v4(best_projector->projmat, co2);
				mul_project_m4_v4(best_projector->projmat, co3);
				if(mf->v4)
					mul_project_m4_v4(best_projector->projmat, co4);

				/* apply transformed coords as UVs */
				tface->uv[0][0] = co1[0];
				tface->uv[0][1] = co1[1];
				tface->uv[1][0] = co2[0];
				tface->uv[1][1] = co2[1];
				tface->uv[2][0] = co3[0];
				tface->uv[2][1] = co3[1];
				if(mf->v4) {
					tface->uv[3][0] = co4[0];
					tface->uv[3][1] = co4[1];
				}
			}
		}

		if(override_image) {
			tface->mode = TF_TEX;
			tface->tpage = image;
		}
	}

	MEM_freeN(coords);

	return dm;
}

static DerivedMesh *uvprojectModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *result;
	UVProjectModifierData *umd = (UVProjectModifierData*) md;

	result = uvprojectModifier_do(umd, ob, derivedData);

	return result;
}

static DerivedMesh *uvprojectModifier_applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return uvprojectModifier_applyModifier(md, ob, derivedData, 0, 1);
}

/* Decimate */

static void decimateModifier_initData(ModifierData *md)
{
	DecimateModifierData *dmd = (DecimateModifierData*) md;

	dmd->percent = 1.0;
}

static void decimateModifier_copyData(ModifierData *md, ModifierData *target)
{
	DecimateModifierData *dmd = (DecimateModifierData*) md;
	DecimateModifierData *tdmd = (DecimateModifierData*) target;

	tdmd->percent = dmd->percent;
}

static DerivedMesh *decimateModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DecimateModifierData *dmd = (DecimateModifierData*) md;
	DerivedMesh *dm = derivedData, *result = NULL;
	MVert *mvert;
	MFace *mface;
	LOD_Decimation_Info lod;
	int totvert, totface;
	int a, numTris;

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);
	totvert = dm->getNumVerts(dm);
	totface = dm->getNumFaces(dm);

	numTris = 0;
	for (a=0; a<totface; a++) {
		MFace *mf = &mface[a];
		numTris++;
		if (mf->v4) numTris++;
	}

	if(numTris<3) {
		modifier_setError(md,
			"Modifier requires more than 3 input faces (triangles).");
		goto exit;
	}

	lod.vertex_buffer= MEM_mallocN(3*sizeof(float)*totvert, "vertices");
	lod.vertex_normal_buffer= MEM_mallocN(3*sizeof(float)*totvert, "normals");
	lod.triangle_index_buffer= MEM_mallocN(3*sizeof(int)*numTris, "trias");
	lod.vertex_num= totvert;
	lod.face_num= numTris;

	for(a=0; a<totvert; a++) {
		MVert *mv = &mvert[a];
		float *vbCo = &lod.vertex_buffer[a*3];
		float *vbNo = &lod.vertex_normal_buffer[a*3];

		VECCOPY(vbCo, mv->co);

		vbNo[0] = mv->no[0]/32767.0f;
		vbNo[1] = mv->no[1]/32767.0f;
		vbNo[2] = mv->no[2]/32767.0f;
	}

	numTris = 0;
	for(a=0; a<totface; a++) {
		MFace *mf = &mface[a];
		int *tri = &lod.triangle_index_buffer[3*numTris++];
		tri[0]= mf->v1;
		tri[1]= mf->v2;
		tri[2]= mf->v3;

		if(mf->v4) {
			tri = &lod.triangle_index_buffer[3*numTris++];
			tri[0]= mf->v1;
			tri[1]= mf->v3;
			tri[2]= mf->v4;
		}
	}

	dmd->faceCount = 0;
	if(LOD_LoadMesh(&lod) ) {
		if( LOD_PreprocessMesh(&lod) ) {
			/* we assume the decim_faces tells how much to reduce */

			while(lod.face_num > numTris*dmd->percent) {
				if( LOD_CollapseEdge(&lod)==0) break;
			}

			if(lod.vertex_num>2) {
				result = CDDM_new(lod.vertex_num, 0, lod.face_num);
				dmd->faceCount = lod.face_num;
			}
			else
				result = CDDM_new(lod.vertex_num, 0, 0);

			mvert = CDDM_get_verts(result);
			for(a=0; a<lod.vertex_num; a++) {
				MVert *mv = &mvert[a];
				float *vbCo = &lod.vertex_buffer[a*3];
				
				VECCOPY(mv->co, vbCo);
			}

			if(lod.vertex_num>2) {
				mface = CDDM_get_faces(result);
				for(a=0; a<lod.face_num; a++) {
					MFace *mf = &mface[a];
					int *tri = &lod.triangle_index_buffer[a*3];
					mf->v1 = tri[0];
					mf->v2 = tri[1];
					mf->v3 = tri[2];
					test_index_face(mf, NULL, 0, 3);
				}
			}

			CDDM_calc_edges(result);
			CDDM_calc_normals(result);
		}
		else
			modifier_setError(md, "Out of memory.");

		LOD_FreeDecimationData(&lod);
	}
	else
		modifier_setError(md, "Non-manifold mesh as input.");

	MEM_freeN(lod.vertex_buffer);
	MEM_freeN(lod.vertex_normal_buffer);
	MEM_freeN(lod.triangle_index_buffer);

exit:
		return result;
}

/* Smooth */

static void smoothModifier_initData(ModifierData *md)
{
	SmoothModifierData *smd = (SmoothModifierData*) md;

	smd->fac = 0.5f;
	smd->repeat = 1;
	smd->flag = MOD_SMOOTH_X | MOD_SMOOTH_Y | MOD_SMOOTH_Z;
	smd->defgrp_name[0] = '\0';
}

static void smoothModifier_copyData(ModifierData *md, ModifierData *target)
{
	SmoothModifierData *smd = (SmoothModifierData*) md;
	SmoothModifierData *tsmd = (SmoothModifierData*) target;

	tsmd->fac = smd->fac;
	tsmd->repeat = smd->repeat;
	tsmd->flag = smd->flag;
	strncpy(tsmd->defgrp_name, smd->defgrp_name, 32);
}

static int smoothModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	SmoothModifierData *smd = (SmoothModifierData*) md;
	short flag;

	flag = smd->flag & (MOD_SMOOTH_X|MOD_SMOOTH_Y|MOD_SMOOTH_Z);

	/* disable if modifier is off for X, Y and Z or if factor is 0 */
	if((smd->fac == 0.0f) || flag == 0) return 1;

	return 0;
}

static CustomDataMask smoothModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	SmoothModifierData *smd = (SmoothModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(smd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void smoothModifier_do(
			      SmoothModifierData *smd, Object *ob, DerivedMesh *dm,
	 float (*vertexCos)[3], int numVerts)
{
	MDeformVert *dvert = NULL;
	MEdge *medges = NULL;

	int i, j, numDMEdges, defgrp_index;
	unsigned char *uctmp;
	float *ftmp, fac, facm;

	ftmp = (float*)MEM_callocN(3*sizeof(float)*numVerts,
		"smoothmodifier_f");
	if (!ftmp) return;
	uctmp = (unsigned char*)MEM_callocN(sizeof(unsigned char)*numVerts,
		 "smoothmodifier_uc");
	if (!uctmp) {
		if (ftmp) MEM_freeN(ftmp);
		return;
	}

	fac = smd->fac;
	facm = 1 - fac;

	medges = dm->getEdgeArray(dm);
	numDMEdges = dm->getNumEdges(dm);

	defgrp_index = -1;

	if (smd->defgrp_name[0]) {
		bDeformGroup *def;

		for (i = 0, def = ob->defbase.first; def; def = def->next, i++) {
			if (!strcmp(def->name, smd->defgrp_name)) {
				defgrp_index = i;
				break;
			}
		}
	}

	if (defgrp_index >= 0)
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

	/* NOTICE: this can be optimized a little bit by moving the
	* if (dvert) out of the loop, if needed */
	for (j = 0; j < smd->repeat; j++) {
		for (i = 0; i < numDMEdges; i++) {
			float fvec[3];
			float *v1, *v2;
			unsigned int idx1, idx2;

			idx1 = medges[i].v1;
			idx2 = medges[i].v2;

			v1 = vertexCos[idx1];
			v2 = vertexCos[idx2];

			fvec[0] = (v1[0] + v2[0]) / 2.0;
			fvec[1] = (v1[1] + v2[1]) / 2.0;
			fvec[2] = (v1[2] + v2[2]) / 2.0;

			v1 = &ftmp[idx1*3];
			v2 = &ftmp[idx2*3];

			if (uctmp[idx1] < 255) {
				uctmp[idx1]++;
				add_v3_v3v3(v1, v1, fvec);
			}
			if (uctmp[idx2] < 255) {
				uctmp[idx2]++;
				add_v3_v3v3(v2, v2, fvec);
			}
		}

		if (dvert) {
			for (i = 0; i < numVerts; i++) {
				MDeformWeight *dw = NULL;
				float f, fm, facw, *fp, *v;
				int k;
				short flag = smd->flag;

				v = vertexCos[i];
				fp = &ftmp[i*3];

				for (k = 0; k < dvert[i].totweight; ++k) {
					if(dvert[i].dw[k].def_nr == defgrp_index) {
						dw = &dvert[i].dw[k];
						break;
					}
				}
				if (!dw) continue;

				f = fac * dw->weight;
				fm = 1.0f - f;

				/* fp is the sum of uctmp[i] verts, so must be averaged */
				facw = 0.0f;
				if (uctmp[i]) 
					facw = f / (float)uctmp[i];

				if (flag & MOD_SMOOTH_X)
					v[0] = fm * v[0] + facw * fp[0];
				if (flag & MOD_SMOOTH_Y)
					v[1] = fm * v[1] + facw * fp[1];
				if (flag & MOD_SMOOTH_Z)
					v[2] = fm * v[2] + facw * fp[2];
			}
		}
		else { /* no vertex group */
			for (i = 0; i < numVerts; i++) {
				float facw, *fp, *v;
				short flag = smd->flag;

				v = vertexCos[i];
				fp = &ftmp[i*3];

				/* fp is the sum of uctmp[i] verts, so must be averaged */
				facw = 0.0f;
				if (uctmp[i]) 
					facw = fac / (float)uctmp[i];

				if (flag & MOD_SMOOTH_X)
					v[0] = facm * v[0] + facw * fp[0];
				if (flag & MOD_SMOOTH_Y)
					v[1] = facm * v[1] + facw * fp[1];
				if (flag & MOD_SMOOTH_Z)
					v[2] = facm * v[2] + facw * fp[2];
			}

		}

		memset(ftmp, 0, 3*sizeof(float)*numVerts);
		memset(uctmp, 0, sizeof(unsigned char)*numVerts);
	}

	MEM_freeN(ftmp);
	MEM_freeN(uctmp);
}

static void smoothModifier_deformVerts(
				       ModifierData *md, Object *ob, DerivedMesh *derivedData,
	   float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm= get_dm(md->scene, ob, NULL, derivedData, NULL, 0);

	smoothModifier_do((SmoothModifierData *)md, ob, dm,
			   vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void smoothModifier_deformVertsEM(
					 ModifierData *md, Object *ob, EditMesh *editData,
      DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm= get_dm(md->scene, ob, editData, derivedData, NULL, 0);

	smoothModifier_do((SmoothModifierData *)md, ob, dm,
			   vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

/* Cast */

static void castModifier_initData(ModifierData *md)
{
	CastModifierData *cmd = (CastModifierData*) md;

	cmd->fac = 0.5f;
	cmd->radius = 0.0f;
	cmd->size = 0.0f;
	cmd->flag = MOD_CAST_X | MOD_CAST_Y | MOD_CAST_Z
			| MOD_CAST_SIZE_FROM_RADIUS;
	cmd->type = MOD_CAST_TYPE_SPHERE;
	cmd->defgrp_name[0] = '\0';
	cmd->object = NULL;
}


static void castModifier_copyData(ModifierData *md, ModifierData *target)
{
	CastModifierData *cmd = (CastModifierData*) md;
	CastModifierData *tcmd = (CastModifierData*) target;

	tcmd->fac = cmd->fac;
	tcmd->radius = cmd->radius;
	tcmd->size = cmd->size;
	tcmd->flag = cmd->flag;
	tcmd->type = cmd->type;
	tcmd->object = cmd->object;
	strncpy(tcmd->defgrp_name, cmd->defgrp_name, 32);
}

static int castModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	CastModifierData *cmd = (CastModifierData*) md;
	short flag;
	
	flag = cmd->flag & (MOD_CAST_X|MOD_CAST_Y|MOD_CAST_Z);

	if((cmd->fac == 0.0f) || flag == 0) return 1;

	return 0;
}

static CustomDataMask castModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	CastModifierData *cmd = (CastModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(cmd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void castModifier_foreachObjectLink(
					   ModifierData *md, Object *ob,
	void (*walk)(void *userData, Object *ob, Object **obpoin),
	       void *userData)
{
	CastModifierData *cmd = (CastModifierData*) md;

	walk (userData, ob, &cmd->object);
}

static void castModifier_updateDepgraph(
					ModifierData *md, DagForest *forest, Scene *scene, Object *ob,
     DagNode *obNode)
{
	CastModifierData *cmd = (CastModifierData*) md;

	if (cmd->object) {
		DagNode *curNode = dag_get_node(forest, cmd->object);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA,
			"Cast Modifier");
	}
}

static void castModifier_sphere_do(
				   CastModifierData *cmd, Object *ob, DerivedMesh *dm,
       float (*vertexCos)[3], int numVerts)
{
	MDeformVert *dvert = NULL;

	Object *ctrl_ob = NULL;

	int i, defgrp_index = -1;
	int has_radius = 0;
	short flag, type;
	float fac, facm, len = 0.0f;
	float vec[3], center[3] = {0.0f, 0.0f, 0.0f};
	float mat[4][4], imat[4][4];

	fac = cmd->fac;
	facm = 1.0f - fac;

	flag = cmd->flag;
	type = cmd->type; /* projection type: sphere or cylinder */

	if (type == MOD_CAST_TYPE_CYLINDER) 
		flag &= ~MOD_CAST_Z;

	ctrl_ob = cmd->object;

	/* spherify's center is {0, 0, 0} (the ob's own center in its local
	* space), by default, but if the user defined a control object,
	* we use its location, transformed to ob's local space */
	if (ctrl_ob) {
		if(flag & MOD_CAST_USE_OB_TRANSFORM) {
			invert_m4_m4(ctrl_ob->imat, ctrl_ob->obmat);
			mul_m4_m4m4(mat, ob->obmat, ctrl_ob->imat);
			invert_m4_m4(imat, mat);
		}

		invert_m4_m4(ob->imat, ob->obmat);
		VECCOPY(center, ctrl_ob->obmat[3]);
		mul_m4_v3(ob->imat, center);
	}

	/* now we check which options the user wants */

	/* 1) (flag was checked in the "if (ctrl_ob)" block above) */
	/* 2) cmd->radius > 0.0f: only the vertices within this radius from
	* the center of the effect should be deformed */
	if (cmd->radius > FLT_EPSILON) has_radius = 1;

	/* 3) if we were given a vertex group name,
	* only those vertices should be affected */
	if (cmd->defgrp_name[0]) {
		bDeformGroup *def;

		for (i = 0, def = ob->defbase.first; def; def = def->next, i++) {
			if (!strcmp(def->name, cmd->defgrp_name)) {
				defgrp_index = i;
				break;
			}
		}
	}

	if ((ob->type == OB_MESH) && dm && defgrp_index >= 0)
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

	if(flag & MOD_CAST_SIZE_FROM_RADIUS) {
		len = cmd->radius;
	}
	else {
		len = cmd->size;
	}

	if(len <= 0) {
		for (i = 0; i < numVerts; i++) {
			len += len_v3v3(center, vertexCos[i]);
		}
		len /= numVerts;

		if (len == 0.0f) len = 10.0f;
	}

	/* ready to apply the effect, one vertex at a time;
	* tiny optimization: the code is separated (with parts repeated)
	 * in two possible cases:
	* with or w/o a vgroup. With lots of if's in the code below,
	* further optimizations are possible, if needed */
	if (dvert) { /* with a vgroup */
		float fac_orig = fac;
		for (i = 0; i < numVerts; i++) {
			MDeformWeight *dw = NULL;
			int j;
			float tmp_co[3];

			VECCOPY(tmp_co, vertexCos[i]);
			if(ctrl_ob) {
				if(flag & MOD_CAST_USE_OB_TRANSFORM) {
					mul_m4_v3(mat, tmp_co);
				} else {
					sub_v3_v3v3(tmp_co, tmp_co, center);
				}
			}

			VECCOPY(vec, tmp_co);

			if (type == MOD_CAST_TYPE_CYLINDER)
				vec[2] = 0.0f;

			if (has_radius) {
				if (len_v3(vec) > cmd->radius) continue;
			}

			for (j = 0; j < dvert[i].totweight; ++j) {
				if(dvert[i].dw[j].def_nr == defgrp_index) {
					dw = &dvert[i].dw[j];
					break;
				}
			}
			if (!dw) continue;

			fac = fac_orig * dw->weight;
			facm = 1.0f - fac;

			normalize_v3(vec);

			if (flag & MOD_CAST_X)
				tmp_co[0] = fac*vec[0]*len + facm*tmp_co[0];
			if (flag & MOD_CAST_Y)
				tmp_co[1] = fac*vec[1]*len + facm*tmp_co[1];
			if (flag & MOD_CAST_Z)
				tmp_co[2] = fac*vec[2]*len + facm*tmp_co[2];

			if(ctrl_ob) {
				if(flag & MOD_CAST_USE_OB_TRANSFORM) {
					mul_m4_v3(imat, tmp_co);
				} else {
					add_v3_v3v3(tmp_co, tmp_co, center);
				}
			}

			VECCOPY(vertexCos[i], tmp_co);
		}
		return;
	}

	/* no vgroup */
	for (i = 0; i < numVerts; i++) {
		float tmp_co[3];

		VECCOPY(tmp_co, vertexCos[i]);
		if(ctrl_ob) {
			if(flag & MOD_CAST_USE_OB_TRANSFORM) {
				mul_m4_v3(mat, tmp_co);
			} else {
				sub_v3_v3v3(tmp_co, tmp_co, center);
			}
		}

		VECCOPY(vec, tmp_co);

		if (type == MOD_CAST_TYPE_CYLINDER)
			vec[2] = 0.0f;

		if (has_radius) {
			if (len_v3(vec) > cmd->radius) continue;
		}

		normalize_v3(vec);

		if (flag & MOD_CAST_X)
			tmp_co[0] = fac*vec[0]*len + facm*tmp_co[0];
		if (flag & MOD_CAST_Y)
			tmp_co[1] = fac*vec[1]*len + facm*tmp_co[1];
		if (flag & MOD_CAST_Z)
			tmp_co[2] = fac*vec[2]*len + facm*tmp_co[2];

		if(ctrl_ob) {
			if(flag & MOD_CAST_USE_OB_TRANSFORM) {
				mul_m4_v3(imat, tmp_co);
			} else {
				add_v3_v3v3(tmp_co, tmp_co, center);
			}
		}

		VECCOPY(vertexCos[i], tmp_co);
	}
}

static void castModifier_cuboid_do(
				   CastModifierData *cmd, Object *ob, DerivedMesh *dm,
       float (*vertexCos)[3], int numVerts)
{
	MDeformVert *dvert = NULL;
	Object *ctrl_ob = NULL;

	int i, defgrp_index = -1;
	int has_radius = 0;
	short flag;
	float fac, facm;
	float min[3], max[3], bb[8][3];
	float center[3] = {0.0f, 0.0f, 0.0f};
	float mat[4][4], imat[4][4];

	fac = cmd->fac;
	facm = 1.0f - fac;

	flag = cmd->flag;

	ctrl_ob = cmd->object;

	/* now we check which options the user wants */

	/* 1) (flag was checked in the "if (ctrl_ob)" block above) */
	/* 2) cmd->radius > 0.0f: only the vertices within this radius from
	* the center of the effect should be deformed */
	if (cmd->radius > FLT_EPSILON) has_radius = 1;

	/* 3) if we were given a vertex group name,
	* only those vertices should be affected */
	if (cmd->defgrp_name[0]) {
		bDeformGroup *def;

		for (i = 0, def = ob->defbase.first; def; def = def->next, i++) {
			if (!strcmp(def->name, cmd->defgrp_name)) {
				defgrp_index = i;
				break;
			}
		}
	}

	if ((ob->type == OB_MESH) && dm && defgrp_index >= 0)
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

	if (ctrl_ob) {
		if(flag & MOD_CAST_USE_OB_TRANSFORM) {
			invert_m4_m4(ctrl_ob->imat, ctrl_ob->obmat);
			mul_m4_m4m4(mat, ob->obmat, ctrl_ob->imat);
			invert_m4_m4(imat, mat);
		}

		invert_m4_m4(ob->imat, ob->obmat);
		VECCOPY(center, ctrl_ob->obmat[3]);
		mul_m4_v3(ob->imat, center);
	}

	if((flag & MOD_CAST_SIZE_FROM_RADIUS) && has_radius) {
		for(i = 0; i < 3; i++) {
			min[i] = -cmd->radius;
			max[i] = cmd->radius;
		}
	} else if(!(flag & MOD_CAST_SIZE_FROM_RADIUS) && cmd->size > 0) {
		for(i = 0; i < 3; i++) {
			min[i] = -cmd->size;
			max[i] = cmd->size;
		}
	} else {
		/* get bound box */
		/* We can't use the object's bound box because other modifiers
		* may have changed the vertex data. */
		INIT_MINMAX(min, max);

		/* Cast's center is the ob's own center in its local space,
		* by default, but if the user defined a control object, we use
		* its location, transformed to ob's local space. */
		if (ctrl_ob) {
			float vec[3];

			/* let the center of the ctrl_ob be part of the bound box: */
			DO_MINMAX(center, min, max);

			for (i = 0; i < numVerts; i++) {
				sub_v3_v3v3(vec, vertexCos[i], center);
				DO_MINMAX(vec, min, max);
			}
		}
		else {
			for (i = 0; i < numVerts; i++) {
				DO_MINMAX(vertexCos[i], min, max);
			}
		}

		/* we want a symmetric bound box around the origin */
		if (fabs(min[0]) > fabs(max[0])) max[0] = fabs(min[0]); 
		if (fabs(min[1]) > fabs(max[1])) max[1] = fabs(min[1]); 
		if (fabs(min[2]) > fabs(max[2])) max[2] = fabs(min[2]);
		min[0] = -max[0];
		min[1] = -max[1];
		min[2] = -max[2];
	}

	/* building our custom bounding box */
	bb[0][0] = bb[2][0] = bb[4][0] = bb[6][0] = min[0];
	bb[1][0] = bb[3][0] = bb[5][0] = bb[7][0] = max[0];
	bb[0][1] = bb[1][1] = bb[4][1] = bb[5][1] = min[1];
	bb[2][1] = bb[3][1] = bb[6][1] = bb[7][1] = max[1];
	bb[0][2] = bb[1][2] = bb[2][2] = bb[3][2] = min[2];
	bb[4][2] = bb[5][2] = bb[6][2] = bb[7][2] = max[2];

	/* ready to apply the effect, one vertex at a time;
	* tiny optimization: the code is separated (with parts repeated)
	 * in two possible cases:
	* with or w/o a vgroup. With lots of if's in the code below,
	* further optimizations are possible, if needed */
	if (dvert) { /* with a vgroup */
		float fac_orig = fac;
		for (i = 0; i < numVerts; i++) {
			MDeformWeight *dw = NULL;
			int j, octant, coord;
			float d[3], dmax, apex[3], fbb;
			float tmp_co[3];

			VECCOPY(tmp_co, vertexCos[i]);
			if(ctrl_ob) {
				if(flag & MOD_CAST_USE_OB_TRANSFORM) {
					mul_m4_v3(mat, tmp_co);
				} else {
					sub_v3_v3v3(tmp_co, tmp_co, center);
				}
			}

			if (has_radius) {
				if (fabs(tmp_co[0]) > cmd->radius ||
								fabs(tmp_co[1]) > cmd->radius ||
								fabs(tmp_co[2]) > cmd->radius) continue;
			}

			for (j = 0; j < dvert[i].totweight; ++j) {
				if(dvert[i].dw[j].def_nr == defgrp_index) {
					dw = &dvert[i].dw[j];
					break;
				}
			}
			if (!dw) continue;

			fac = fac_orig * dw->weight;
			facm = 1.0f - fac;

			/* The algo used to project the vertices to their
			 * bounding box (bb) is pretty simple:
			 * for each vertex v:
			* 1) find in which octant v is in;
			* 2) find which outer "wall" of that octant is closer to v;
			* 3) calculate factor (var fbb) to project v to that wall;
			* 4) project. */

			/* find in which octant this vertex is in */
			octant = 0;
			if (tmp_co[0] > 0.0f) octant += 1;
			if (tmp_co[1] > 0.0f) octant += 2;
			if (tmp_co[2] > 0.0f) octant += 4;

			/* apex is the bb's vertex at the chosen octant */
			copy_v3_v3(apex, bb[octant]);

			/* find which bb plane is closest to this vertex ... */
			d[0] = tmp_co[0] / apex[0];
			d[1] = tmp_co[1] / apex[1];
			d[2] = tmp_co[2] / apex[2];

			/* ... (the closest has the higher (closer to 1) d value) */
			dmax = d[0];
			coord = 0;
			if (d[1] > dmax) {
				dmax = d[1];
				coord = 1;
			}
			if (d[2] > dmax) {
				/* dmax = d[2]; */ /* commented, we don't need it */
				coord = 2;
			}

			/* ok, now we know which coordinate of the vertex to use */

			if (fabs(tmp_co[coord]) < FLT_EPSILON) /* avoid division by zero */
				continue;

			/* finally, this is the factor we wanted, to project the vertex
			* to its bounding box (bb) */
			fbb = apex[coord] / tmp_co[coord];

			/* calculate the new vertex position */
			if (flag & MOD_CAST_X)
				tmp_co[0] = facm * tmp_co[0] + fac * tmp_co[0] * fbb;
			if (flag & MOD_CAST_Y)
				tmp_co[1] = facm * tmp_co[1] + fac * tmp_co[1] * fbb;
			if (flag & MOD_CAST_Z)
				tmp_co[2] = facm * tmp_co[2] + fac * tmp_co[2] * fbb;

			if(ctrl_ob) {
				if(flag & MOD_CAST_USE_OB_TRANSFORM) {
					mul_m4_v3(imat, tmp_co);
				} else {
					add_v3_v3v3(tmp_co, tmp_co, center);
				}
			}

			VECCOPY(vertexCos[i], tmp_co);
		}
		return;
	}

	/* no vgroup (check previous case for comments about the code) */
	for (i = 0; i < numVerts; i++) {
		int octant, coord;
		float d[3], dmax, fbb, apex[3];
		float tmp_co[3];

		VECCOPY(tmp_co, vertexCos[i]);
		if(ctrl_ob) {
			if(flag & MOD_CAST_USE_OB_TRANSFORM) {
				mul_m4_v3(mat, tmp_co);
			} else {
				sub_v3_v3v3(tmp_co, tmp_co, center);
			}
		}

		if (has_radius) {
			if (fabs(tmp_co[0]) > cmd->radius ||
						 fabs(tmp_co[1]) > cmd->radius ||
						 fabs(tmp_co[2]) > cmd->radius) continue;
		}

		octant = 0;
		if (tmp_co[0] > 0.0f) octant += 1;
		if (tmp_co[1] > 0.0f) octant += 2;
		if (tmp_co[2] > 0.0f) octant += 4;

		copy_v3_v3(apex, bb[octant]);

		d[0] = tmp_co[0] / apex[0];
		d[1] = tmp_co[1] / apex[1];
		d[2] = tmp_co[2] / apex[2];

		dmax = d[0];
		coord = 0;
		if (d[1] > dmax) {
			dmax = d[1];
			coord = 1;
		}
		if (d[2] > dmax) {
			/* dmax = d[2]; */ /* commented, we don't need it */
			coord = 2;
		}

		if (fabs(tmp_co[coord]) < FLT_EPSILON)
			continue;

		fbb = apex[coord] / tmp_co[coord];

		if (flag & MOD_CAST_X)
			tmp_co[0] = facm * tmp_co[0] + fac * tmp_co[0] * fbb;
		if (flag & MOD_CAST_Y)
			tmp_co[1] = facm * tmp_co[1] + fac * tmp_co[1] * fbb;
		if (flag & MOD_CAST_Z)
			tmp_co[2] = facm * tmp_co[2] + fac * tmp_co[2] * fbb;

		if(ctrl_ob) {
			if(flag & MOD_CAST_USE_OB_TRANSFORM) {
				mul_m4_v3(imat, tmp_co);
			} else {
				add_v3_v3v3(tmp_co, tmp_co, center);
			}
		}

		VECCOPY(vertexCos[i], tmp_co);
	}
}

static void castModifier_deformVerts(
				     ModifierData *md, Object *ob, DerivedMesh *derivedData,
	 float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = get_dm(md->scene, ob, NULL, derivedData, NULL, 0);
	CastModifierData *cmd = (CastModifierData *)md;

	if (cmd->type == MOD_CAST_TYPE_CUBOID) {
		castModifier_cuboid_do(cmd, ob, dm, vertexCos, numVerts);
	} else { /* MOD_CAST_TYPE_SPHERE or MOD_CAST_TYPE_CYLINDER */
		castModifier_sphere_do(cmd, ob, dm, vertexCos, numVerts);
	}

	if(dm != derivedData)
		dm->release(dm);
}

static void castModifier_deformVertsEM(
				       ModifierData *md, Object *ob, EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = get_dm(md->scene, ob, editData, derivedData, NULL, 0);
	CastModifierData *cmd = (CastModifierData *)md;

	if (cmd->type == MOD_CAST_TYPE_CUBOID) {
		castModifier_cuboid_do(cmd, ob, dm, vertexCos, numVerts);
	} else { /* MOD_CAST_TYPE_SPHERE or MOD_CAST_TYPE_CYLINDER */
		castModifier_sphere_do(cmd, ob, dm, vertexCos, numVerts);
	}

	if(dm != derivedData)
		dm->release(dm);
}

/* Wave */

static void waveModifier_initData(ModifierData *md)
{
	WaveModifierData *wmd = (WaveModifierData*) md; // whadya know, moved here from Iraq

	wmd->flag |= (MOD_WAVE_X | MOD_WAVE_Y | MOD_WAVE_CYCL
			| MOD_WAVE_NORM_X | MOD_WAVE_NORM_Y | MOD_WAVE_NORM_Z);

	wmd->objectcenter = NULL;
	wmd->texture = NULL;
	wmd->map_object = NULL;
	wmd->height= 0.5f;
	wmd->width= 1.5f;
	wmd->speed= 0.25f;
	wmd->narrow= 1.5f;
	wmd->lifetime= 0.0f;
	wmd->damp= 10.0f;
	wmd->falloff= 0.0f;
	wmd->texmapping = MOD_WAV_MAP_LOCAL;
	wmd->defgrp_name[0] = 0;
}

static void waveModifier_copyData(ModifierData *md, ModifierData *target)
{
	WaveModifierData *wmd = (WaveModifierData*) md;
	WaveModifierData *twmd = (WaveModifierData*) target;

	twmd->damp = wmd->damp;
	twmd->flag = wmd->flag;
	twmd->height = wmd->height;
	twmd->lifetime = wmd->lifetime;
	twmd->narrow = wmd->narrow;
	twmd->speed = wmd->speed;
	twmd->startx = wmd->startx;
	twmd->starty = wmd->starty;
	twmd->timeoffs = wmd->timeoffs;
	twmd->width = wmd->width;
	twmd->falloff = wmd->falloff;
	twmd->objectcenter = wmd->objectcenter;
	twmd->texture = wmd->texture;
	twmd->map_object = wmd->map_object;
	twmd->texmapping = wmd->texmapping;
	strncpy(twmd->defgrp_name, wmd->defgrp_name, 32);
}

static int waveModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static void waveModifier_foreachObjectLink(
					   ModifierData *md, Object *ob,
	ObjectWalkFunc walk, void *userData)
{
	WaveModifierData *wmd = (WaveModifierData*) md;

	walk(userData, ob, &wmd->objectcenter);
	walk(userData, ob, &wmd->map_object);
}

static void waveModifier_foreachIDLink(ModifierData *md, Object *ob,
				       IDWalkFunc walk, void *userData)
{
	WaveModifierData *wmd = (WaveModifierData*) md;

	walk(userData, ob, (ID **)&wmd->texture);

	waveModifier_foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void waveModifier_updateDepgraph(
					ModifierData *md, DagForest *forest, Scene *scene, Object *ob,
     DagNode *obNode)
{
	WaveModifierData *wmd = (WaveModifierData*) md;

	if(wmd->objectcenter) {
		DagNode *curNode = dag_get_node(forest, wmd->objectcenter);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA,
			"Wave Modifier");
	}

	if(wmd->map_object) {
		DagNode *curNode = dag_get_node(forest, wmd->map_object);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA,
			"Wave Modifer");
	}
}

static CustomDataMask waveModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	WaveModifierData *wmd = (WaveModifierData *)md;
	CustomDataMask dataMask = 0;


	/* ask for UV coordinates if we need them */
	if(wmd->texture && wmd->texmapping == MOD_WAV_MAP_UV)
		dataMask |= (1 << CD_MTFACE);

	/* ask for vertexgroups if we need them */
	if(wmd->defgrp_name[0])
		dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void wavemod_get_texture_coords(WaveModifierData *wmd, Object *ob,
				       DerivedMesh *dm,
	   float (*co)[3], float (*texco)[3],
		   int numVerts)
{
	int i;
	int texmapping = wmd->texmapping;

	if(texmapping == MOD_WAV_MAP_OBJECT) {
		if(wmd->map_object)
			invert_m4_m4(wmd->map_object->imat, wmd->map_object->obmat);
		else /* if there is no map object, default to local */
			texmapping = MOD_WAV_MAP_LOCAL;
	}

	/* UVs need special handling, since they come from faces */
	if(texmapping == MOD_WAV_MAP_UV) {
		if(CustomData_has_layer(&dm->faceData, CD_MTFACE)) {
			MFace *mface = dm->getFaceArray(dm);
			MFace *mf;
			char *done = MEM_callocN(sizeof(*done) * numVerts,
					"get_texture_coords done");
			int numFaces = dm->getNumFaces(dm);
			char uvname[32];
			MTFace *tf;

			validate_layer_name(&dm->faceData, CD_MTFACE, wmd->uvlayer_name, uvname);
			tf = CustomData_get_layer_named(&dm->faceData, CD_MTFACE, uvname);

			/* verts are given the UV from the first face that uses them */
			for(i = 0, mf = mface; i < numFaces; ++i, ++mf, ++tf) {
				if(!done[mf->v1]) {
					texco[mf->v1][0] = tf->uv[0][0];
					texco[mf->v1][1] = tf->uv[0][1];
					texco[mf->v1][2] = 0;
					done[mf->v1] = 1;
				}
				if(!done[mf->v2]) {
					texco[mf->v2][0] = tf->uv[1][0];
					texco[mf->v2][1] = tf->uv[1][1];
					texco[mf->v2][2] = 0;
					done[mf->v2] = 1;
				}
				if(!done[mf->v3]) {
					texco[mf->v3][0] = tf->uv[2][0];
					texco[mf->v3][1] = tf->uv[2][1];
					texco[mf->v3][2] = 0;
					done[mf->v3] = 1;
				}
				if(!done[mf->v4]) {
					texco[mf->v4][0] = tf->uv[3][0];
					texco[mf->v4][1] = tf->uv[3][1];
					texco[mf->v4][2] = 0;
					done[mf->v4] = 1;
				}
			}

			/* remap UVs from [0, 1] to [-1, 1] */
			for(i = 0; i < numVerts; ++i) {
				texco[i][0] = texco[i][0] * 2 - 1;
				texco[i][1] = texco[i][1] * 2 - 1;
			}

			MEM_freeN(done);
			return;
		} else /* if there are no UVs, default to local */
			texmapping = MOD_WAV_MAP_LOCAL;
	}

	for(i = 0; i < numVerts; ++i, ++co, ++texco) {
		switch(texmapping) {
			case MOD_WAV_MAP_LOCAL:
				VECCOPY(*texco, *co);
				break;
			case MOD_WAV_MAP_GLOBAL:
				VECCOPY(*texco, *co);
				mul_m4_v3(ob->obmat, *texco);
				break;
			case MOD_WAV_MAP_OBJECT:
				VECCOPY(*texco, *co);
				mul_m4_v3(ob->obmat, *texco);
				mul_m4_v3(wmd->map_object->imat, *texco);
				break;
		}
	}
}

static void waveModifier_do(WaveModifierData *md, 
		Scene *scene, Object *ob, DerivedMesh *dm,
       float (*vertexCos)[3], int numVerts)
{
	WaveModifierData *wmd = (WaveModifierData*) md;
	MVert *mvert = NULL;
	MDeformVert *dvert = NULL;
	int defgrp_index;
	float ctime = bsystem_time(scene, ob, (float)scene->r.cfra, 0.0);
	float minfac =
			(float)(1.0 / exp(wmd->width * wmd->narrow * wmd->width * wmd->narrow));
	float lifefac = wmd->height;
	float (*tex_co)[3] = NULL;

	if(wmd->flag & MOD_WAVE_NORM && ob->type == OB_MESH)
		mvert = dm->getVertArray(dm);

	if(wmd->objectcenter){
		float mat[4][4];
		/* get the control object's location in local coordinates */
		invert_m4_m4(ob->imat, ob->obmat);
		mul_m4_m4m4(mat, wmd->objectcenter->obmat, ob->imat);

		wmd->startx = mat[3][0];
		wmd->starty = mat[3][1];
	}

	/* get the index of the deform group */
	defgrp_index = -1;

	if(wmd->defgrp_name[0]) {
		int i;
		bDeformGroup *def;
		for(i = 0, def = ob->defbase.first; def; def = def->next, i++) {
			if(!strcmp(def->name, wmd->defgrp_name)) {
				defgrp_index = i;
				break;
			}
		}
	}

	if(defgrp_index >= 0){
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
	}

	if(wmd->damp == 0) wmd->damp = 10.0f;

	if(wmd->lifetime != 0.0) {
		float x = ctime - wmd->timeoffs;

		if(x > wmd->lifetime) {
			lifefac = x - wmd->lifetime;

			if(lifefac > wmd->damp) lifefac = 0.0;
			else lifefac =
				(float)(wmd->height * (1.0 - sqrt(lifefac / wmd->damp)));
		}
	}

	if(wmd->texture) {
		tex_co = MEM_mallocN(sizeof(*tex_co) * numVerts,
				     "waveModifier_do tex_co");
		wavemod_get_texture_coords(wmd, ob, dm, vertexCos, tex_co, numVerts);
	}

	if(lifefac != 0.0) {
		int i;

		for(i = 0; i < numVerts; i++) {
			float *co = vertexCos[i];
			float x = co[0] - wmd->startx;
			float y = co[1] - wmd->starty;
			float amplit= 0.0f;
			float dist = 0.0f;
			float falloff_fac = 0.0f;
			TexResult texres;
			MDeformWeight *def_weight = NULL;

			/* get weights */
			if(dvert) {
				int j;
				for(j = 0; j < dvert[i].totweight; ++j) {
					if(dvert[i].dw[j].def_nr == defgrp_index) {
						def_weight = &dvert[i].dw[j];
						break;
					}
				}

				/* if this vert isn't in the vgroup, don't deform it */
				if(!def_weight) continue;
			}

			if(wmd->texture) {
				texres.nor = NULL;
				get_texture_value(wmd->texture, tex_co[i], &texres);
			}

			/*get dist*/
			if(wmd->flag & MOD_WAVE_X) {
				if(wmd->flag & MOD_WAVE_Y){
					dist = (float)sqrt(x*x + y*y);
				}
				else{
					dist = fabs(x);
				}
			}
			else if(wmd->flag & MOD_WAVE_Y) {
				dist = fabs(y);
			}

			falloff_fac = (1.0-(dist / wmd->falloff));
			CLAMP(falloff_fac,0,1);

			if(wmd->flag & MOD_WAVE_X) {
				if(wmd->flag & MOD_WAVE_Y) amplit = (float)sqrt(x*x + y*y);
				else amplit = x;
			}
			else if(wmd->flag & MOD_WAVE_Y)
				amplit= y;

			/* this way it makes nice circles */
			amplit -= (ctime - wmd->timeoffs) * wmd->speed;

			if(wmd->flag & MOD_WAVE_CYCL) {
				amplit = (float)fmod(amplit - wmd->width, 2.0 * wmd->width)
						+ wmd->width;
			}

			/* GAUSSIAN */
			if(amplit > -wmd->width && amplit < wmd->width) {
				amplit = amplit * wmd->narrow;
				amplit = (float)(1.0 / exp(amplit * amplit) - minfac);

				/*apply texture*/
				if(wmd->texture)
					amplit = amplit * texres.tin;

				/*apply weight*/
				if(def_weight)
					amplit = amplit * def_weight->weight;

				/*apply falloff*/
				if (wmd->falloff > 0)
					amplit = amplit * falloff_fac;

				if(mvert) {
					/* move along normals */
					if(wmd->flag & MOD_WAVE_NORM_X) {
						co[0] += (lifefac * amplit) * mvert[i].no[0] / 32767.0f;
					}
					if(wmd->flag & MOD_WAVE_NORM_Y) {
						co[1] += (lifefac * amplit) * mvert[i].no[1] / 32767.0f;
					}
					if(wmd->flag & MOD_WAVE_NORM_Z) {
						co[2] += (lifefac * amplit) * mvert[i].no[2] / 32767.0f;
					}
				}
				else {
					/* move along local z axis */
					co[2] += lifefac * amplit;
				}
			}
		}
	}

	if(wmd->texture) MEM_freeN(tex_co);
}

static void waveModifier_deformVerts(
				     ModifierData *md, Object *ob, DerivedMesh *derivedData,
	 float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm= derivedData;
	WaveModifierData *wmd = (WaveModifierData *)md;

	if(wmd->flag & MOD_WAVE_NORM)
		dm= get_cddm(md->scene, ob, NULL, dm, vertexCos);
	else if(wmd->texture || wmd->defgrp_name[0])
		dm= get_dm(md->scene, ob, NULL, dm, NULL, 0);

	waveModifier_do(wmd, md->scene, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void waveModifier_deformVertsEM(
				       ModifierData *md, Object *ob, EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm= derivedData;
	WaveModifierData *wmd = (WaveModifierData *)md;

	if(wmd->flag & MOD_WAVE_NORM)
		dm= get_cddm(md->scene, ob, editData, dm, vertexCos);
	else if(wmd->texture || wmd->defgrp_name[0])
		dm= get_dm(md->scene, ob, editData, dm, NULL, 0);

	waveModifier_do(wmd, md->scene, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

/* Armature */

static void armatureModifier_initData(ModifierData *md)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;
	
	amd->deformflag = ARM_DEF_ENVELOPE | ARM_DEF_VGROUP;
}

static void armatureModifier_copyData(ModifierData *md, ModifierData *target)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;
	ArmatureModifierData *tamd = (ArmatureModifierData*) target;

	tamd->object = amd->object;
	tamd->deformflag = amd->deformflag;
	strncpy(tamd->defgrp_name, amd->defgrp_name, 32);
}

static CustomDataMask armatureModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups */
	dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static int armatureModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	return !amd->object;
}

static void armatureModifier_foreachObjectLink(
					       ModifierData *md, Object *ob,
	    void (*walk)(void *userData, Object *ob, Object **obpoin),
		   void *userData)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	walk(userData, ob, &amd->object);
}

static void armatureModifier_updateDepgraph(
					    ModifierData *md, DagForest *forest, Scene *scene, Object *ob,
	 DagNode *obNode)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	if (amd->object) {
		DagNode *curNode = dag_get_node(forest, amd->object);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Armature Modifier");
	}
}

static void armatureModifier_deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
      float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;

	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */
	
	armature_deform_verts(amd->object, ob, derivedData, vertexCos, NULL,
			      numVerts, amd->deformflag, 
	 (float(*)[3])amd->prevCos, amd->defgrp_name);
	/* free cache */
	if(amd->prevCos) {
		MEM_freeN(amd->prevCos);
		amd->prevCos= NULL;
	}
}

static void armatureModifier_deformVertsEM(
					   ModifierData *md, Object *ob, EditMesh *editData,
	DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;
	DerivedMesh *dm = derivedData;

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	armature_deform_verts(amd->object, ob, dm, vertexCos, NULL, numVerts,
			      amd->deformflag, NULL, amd->defgrp_name);

	if(!derivedData) dm->release(dm);
}

static void armatureModifier_deformMatricesEM(
					      ModifierData *md, Object *ob, EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3],
					     float (*defMats)[3][3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData*) md;
	DerivedMesh *dm = derivedData;

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	armature_deform_verts(amd->object, ob, dm, vertexCos, defMats, numVerts,
			      amd->deformflag, NULL, amd->defgrp_name);

	if(!derivedData) dm->release(dm);
}

/* Hook */

static void hookModifier_initData(ModifierData *md) 
{
	HookModifierData *hmd = (HookModifierData*) md;

	hmd->force= 1.0;
}

static void hookModifier_copyData(ModifierData *md, ModifierData *target)
{
	HookModifierData *hmd = (HookModifierData*) md;
	HookModifierData *thmd = (HookModifierData*) target;

	VECCOPY(thmd->cent, hmd->cent);
	thmd->falloff = hmd->falloff;
	thmd->force = hmd->force;
	thmd->object = hmd->object;
	thmd->totindex = hmd->totindex;
	thmd->indexar = MEM_dupallocN(hmd->indexar);
	memcpy(thmd->parentinv, hmd->parentinv, sizeof(hmd->parentinv));
	strncpy(thmd->name, hmd->name, 32);
	strncpy(thmd->subtarget, hmd->subtarget, 32);
}

static CustomDataMask hookModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(!hmd->indexar && hmd->name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void hookModifier_freeData(ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData*) md;

	if (hmd->indexar) MEM_freeN(hmd->indexar);
}

static int hookModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	HookModifierData *hmd = (HookModifierData*) md;

	return !hmd->object;
}

static void hookModifier_foreachObjectLink(
					   ModifierData *md, Object *ob,
	void (*walk)(void *userData, Object *ob, Object **obpoin),
	       void *userData)
{
	HookModifierData *hmd = (HookModifierData*) md;

	walk(userData, ob, &hmd->object);
}

static void hookModifier_updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene,
					Object *ob, DagNode *obNode)
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

static void hookModifier_deformVerts(
				     ModifierData *md, Object *ob, DerivedMesh *derivedData,
	 float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	HookModifierData *hmd = (HookModifierData*) md;
	bPoseChannel *pchan= get_pose_channel(hmd->object->pose, hmd->subtarget);
	float vec[3], mat[4][4], dmat[4][4];
	int i;
	DerivedMesh *dm = derivedData;
	
	/* get world-space matrix of target, corrected for the space the verts are in */
	if (hmd->subtarget[0] && pchan) {
		/* bone target if there's a matching pose-channel */
		mul_m4_m4m4(dmat, pchan->pose_mat, hmd->object->obmat);
	}
	else {
		/* just object target */
		copy_m4_m4(dmat, hmd->object->obmat);
	}
	invert_m4_m4(ob->imat, ob->obmat);
	mul_serie_m4(mat, ob->imat, dmat, hmd->parentinv,
		     NULL, NULL, NULL, NULL, NULL);

	/* vertex indices? */
	if(hmd->indexar) {
		for(i = 0; i < hmd->totindex; i++) {
			int index = hmd->indexar[i];

			/* This should always be true and I don't generally like 
			* "paranoid" style code like this, but old files can have
			* indices that are out of range because old blender did
			* not correct them on exit editmode. - zr
			*/
			if(index < numVerts) {
				float *co = vertexCos[index];
				float fac = hmd->force;

				/* if DerivedMesh is present and has original index data,
				* use it
				*/
				if(dm && dm->getVertDataArray(dm, CD_ORIGINDEX)) {
					int j;
					int orig_index;
					for(j = 0; j < numVerts; ++j) {
						fac = hmd->force;
						orig_index = *(int *)dm->getVertData(dm, j,
								CD_ORIGINDEX);
						if(orig_index == index) {
							co = vertexCos[j];
							if(hmd->falloff != 0.0) {
								float len = len_v3v3(co, hmd->cent);
								if(len > hmd->falloff) fac = 0.0;
								else if(len > 0.0)
									fac *= sqrt(1.0 - len / hmd->falloff);
							}

							if(fac != 0.0) {
								mul_v3_m4v3(vec, mat, co);
								interp_v3_v3v3(co, co, vec, fac);
							}
						}
					}
				} else {
					if(hmd->falloff != 0.0) {
						float len = len_v3v3(co, hmd->cent);
						if(len > hmd->falloff) fac = 0.0;
						else if(len > 0.0)
							fac *= sqrt(1.0 - len / hmd->falloff);
					}

					if(fac != 0.0) {
						mul_v3_m4v3(vec, mat, co);
						interp_v3_v3v3(co, co, vec, fac);
					}
				}
			}
		}
	} 
	else if(hmd->name[0]) {	/* vertex group hook */
		bDeformGroup *curdef;
		Mesh *me = ob->data;
		int index = 0;
		int use_dverts;
		int maxVerts = 0;
		
		/* find the group (weak loop-in-loop) */
		for(curdef = ob->defbase.first; curdef; curdef = curdef->next, index++)
			if(!strcmp(curdef->name, hmd->name)) break;

		if(dm)
			if(dm->getVertData(dm, 0, CD_MDEFORMVERT)) {
			use_dverts = 1;
			maxVerts = dm->getNumVerts(dm);
			} else use_dverts = 0;
			else if(me->dvert) {
				use_dverts = 1;
				maxVerts = me->totvert;
			} else use_dverts = 0;
		
			if(curdef && use_dverts) {
				MDeformVert *dvert = me->dvert;
				int i, j;
			
				for(i = 0; i < maxVerts; i++, dvert++) {
					if(dm) dvert = dm->getVertData(dm, i, CD_MDEFORMVERT);
					for(j = 0; j < dvert->totweight; j++) {
						if(dvert->dw[j].def_nr == index) {
							float fac = hmd->force*dvert->dw[j].weight;
							float *co = vertexCos[i];
						
							if(hmd->falloff != 0.0) {
								float len = len_v3v3(co, hmd->cent);
								if(len > hmd->falloff) fac = 0.0;
								else if(len > 0.0)
									fac *= sqrt(1.0 - len / hmd->falloff);
							}
						
							mul_v3_m4v3(vec, mat, co);
							interp_v3_v3v3(co, co, vec, fac);
						}
					}
				}
			}
	}
}

static void hookModifier_deformVertsEM(
				       ModifierData *md, Object *ob, EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	hookModifier_deformVerts(md, ob, derivedData, vertexCos, numVerts, 0, 0);

	if(!derivedData) dm->release(dm);
}

/* Softbody */

static void softbodyModifier_deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
      float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	sbObjectStep(md->scene, ob, (float)md->scene->r.cfra, vertexCos, numVerts);
}

static int softbodyModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

/* Solidify */


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
				VECCOPY(edge_normal, face_nors[edge_ref->f1])
				mul_v3_fl(edge_normal, M_PI/2);
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
 
static void solidifyModifier_initData(ModifierData *md)
{
	SolidifyModifierData *smd = (SolidifyModifierData*) md;
	smd->offset = 0.01f;
	smd->flag = MOD_SOLIDIFY_RIM;
}
 
static void solidifyModifier_copyData(ModifierData *md, ModifierData *target)
{
	SolidifyModifierData *smd = (SolidifyModifierData*) md;
	SolidifyModifierData *tsmd = (SolidifyModifierData*) target;
	tsmd->offset = smd->offset;
	tsmd->crease_inner = smd->crease_inner;
	tsmd->crease_outer = smd->crease_outer;
	tsmd->crease_rim = smd->crease_rim;
	strcpy(tsmd->vgroup, smd->vgroup);
}

static DerivedMesh *solidifyModifier_applyModifier(ModifierData *md,
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
			edge_users[eidx]= (_v1 < _v2) ? i:(i+numFaces); \
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

	if((smd->flag & MOD_SOLIDIFY_EVEN) == 0) {
		/* no even thickness, very simple */
		float scalar_short = smd->offset / 32767.0f;

		if(smd->offset < 0.0f)	mv= mvert+numVerts;
		else					mv= mvert;

		for(i=0; i<numVerts; i++, mv++) {
			mv->co[0] += mv->no[0] * scalar_short;
			mv->co[1] += mv->no[1] * scalar_short;
			mv->co[2] += mv->no[2] * scalar_short;
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

		if(smd->offset < 0.0f)	mv= mvert+numVerts;
		else					mv= mvert;

		for(i=0; i<numVerts; i++, mv++) {
			if(vert_accum[i]) { /* zero if unselected */
				madd_v3_v3fl(mv->co, vert_nors[i], smd->offset * (vert_angles[i] / vert_accum[i]));
			}
		}

		MEM_freeN(vert_angles);
	}

	if(vert_nors)
		MEM_freeN(vert_nors);

	if(smd->flag & MOD_SOLIDIFY_RIM) {

		static int edge_indices[4][4] = {
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

			if(smd->crease_rim)
				ed->crease= smd->crease_rim * 255.0f;
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

			if(smd->crease_outer > 0.0f)
				ed->crease= smd->crease_outer * 255.0f;

			if(smd->crease_inner > 0.0f) {
				ed= medge + (numEdges + eidx);
				ed->crease= smd->crease_inner * 255.0f;
			}
		}

		MEM_freeN(new_vert_arr);
		MEM_freeN(new_edge_arr);
		MEM_freeN(edge_users);
		MEM_freeN(edge_order);
	}

	return result;
}

static DerivedMesh *solidifyModifier_applyModifierEM(ModifierData *md,
						     Object *ob,
						     EditMesh *editData,
						     DerivedMesh *derivedData)
{
	return solidifyModifier_applyModifier(md, ob, derivedData, 0, 1);
}

/* Smoke */

static void smokeModifier_initData(ModifierData *md) 
{
	SmokeModifierData *smd = (SmokeModifierData*) md;
	
	smd->domain = NULL;
	smd->flow = NULL;
	smd->coll = NULL;
	smd->type = 0;
	smd->time = -1;
}

static void smokeModifier_freeData(ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData*) md;
	
	smokeModifier_free (smd);
}

static void smokeModifier_deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
      float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	SmokeModifierData *smd = (SmokeModifierData*) md;
	DerivedMesh *dm = dm= get_cddm(md->scene, ob, NULL, derivedData, vertexCos);

	smokeModifier_do(smd, md->scene, ob, dm, useRenderParams, isFinalCalc);

	if(dm != derivedData)
		dm->release(dm);
}

static int smokeModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static void smokeModifier_updateDepgraph(
					 ModifierData *md, DagForest *forest, Scene *scene, Object *ob,
      DagNode *obNode)
{
	/*SmokeModifierData *smd = (SmokeModifierData *) md;
	if(smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain)
	{
		if(smd->domain->fluid_group)
		{
			GroupObject *go = NULL;
			
			for(go = smd->domain->fluid_group->gobject.first; go; go = go->next) 
			{
				if(go->ob)
				{
					SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(go->ob, eModifierType_Smoke);
					
					// check for initialized smoke object
					if(smd2 && (smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow)
					{
						DagNode *curNode = dag_get_node(forest, go->ob);
						dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Smoke Flow");
					}
				}
			}
		}
	}
	*/
}

/* Cloth */

static void clothModifier_initData(ModifierData *md) 
{
	ClothModifierData *clmd = (ClothModifierData*) md;
	
	clmd->sim_parms = MEM_callocN(sizeof(ClothSimSettings), "cloth sim parms");
	clmd->coll_parms = MEM_callocN(sizeof(ClothCollSettings), "cloth coll parms");
	clmd->point_cache = BKE_ptcache_add(&clmd->ptcaches);
	
	/* check for alloc failing */
	if(!clmd->sim_parms || !clmd->coll_parms || !clmd->point_cache)
		return;
	
	cloth_init (clmd);
}

static DerivedMesh *clothModifier_applyModifier(ModifierData *md, Object *ob,
		DerivedMesh *derivedData, int useRenderParams, int isFinalCalc)
{
	ClothModifierData *clmd = (ClothModifierData*) md;
	DerivedMesh *result=NULL;
	
	/* check for alloc failing */
	if(!clmd->sim_parms || !clmd->coll_parms)
	{
		clothModifier_initData(md);
		
		if(!clmd->sim_parms || !clmd->coll_parms)
			return derivedData;
	}

	result = clothModifier_do(clmd, md->scene, ob, derivedData, useRenderParams, isFinalCalc);

	if(result)
	{
		CDDM_calc_normals(result);
		return result;
	}
	return derivedData;
}

static void clothModifier_updateDepgraph(
					 ModifierData *md, DagForest *forest, Scene *scene, Object *ob,
      DagNode *obNode)
{
	ClothModifierData *clmd = (ClothModifierData*) md;
	
	Base *base;
	
	if(clmd)
	{
		for(base = scene->base.first; base; base= base->next) 
		{
			Object *ob1= base->object;
			if(ob1 != ob)
			{
				CollisionModifierData *coll_clmd = (CollisionModifierData *)modifiers_findByType(ob1, eModifierType_Collision);
				if(coll_clmd)
				{
					DagNode *curNode = dag_get_node(forest, ob1);
					dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Cloth Collision");
				}
			}
		}
	}
}

static CustomDataMask clothModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void clothModifier_copyData(ModifierData *md, ModifierData *target)
{
	ClothModifierData *clmd = (ClothModifierData*) md;
	ClothModifierData *tclmd = (ClothModifierData*) target;
	
	if(tclmd->sim_parms)
		MEM_freeN(tclmd->sim_parms);
	if(tclmd->coll_parms)
		MEM_freeN(tclmd->coll_parms);
	
	BKE_ptcache_free_list(&tclmd->ptcaches);
	tclmd->point_cache = NULL;
	
	tclmd->sim_parms = MEM_dupallocN(clmd->sim_parms);
	if(clmd->sim_parms->effector_weights)
		tclmd->sim_parms->effector_weights = MEM_dupallocN(clmd->sim_parms->effector_weights);
	tclmd->coll_parms = MEM_dupallocN(clmd->coll_parms);
	tclmd->point_cache = BKE_ptcache_copy_list(&tclmd->ptcaches, &clmd->ptcaches);
	tclmd->clothObject = NULL;
}

static int clothModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static void clothModifier_freeData(ModifierData *md)
{
	ClothModifierData *clmd = (ClothModifierData*) md;
	
	if (clmd) 
	{
		if(G.rt > 0)
			printf("clothModifier_freeData\n");
		
		cloth_free_modifier_extern (clmd);
		
		if(clmd->sim_parms) {
			if(clmd->sim_parms->effector_weights)
				MEM_freeN(clmd->sim_parms->effector_weights);
			MEM_freeN(clmd->sim_parms);
		}
		if(clmd->coll_parms)
			MEM_freeN(clmd->coll_parms);	
		
		BKE_ptcache_free_list(&clmd->ptcaches);
		clmd->point_cache = NULL;
	}
}

/* Collision */

static void collisionModifier_initData(ModifierData *md) 
{
	CollisionModifierData *collmd = (CollisionModifierData*) md;
	
	collmd->x = NULL;
	collmd->xnew = NULL;
	collmd->current_x = NULL;
	collmd->current_xnew = NULL;
	collmd->current_v = NULL;
	collmd->time = -1000;
	collmd->numverts = 0;
	collmd->bvhtree = NULL;
}

static void collisionModifier_freeData(ModifierData *md)
{
	CollisionModifierData *collmd = (CollisionModifierData*) md;
	
	if (collmd) 
	{
		if(collmd->bvhtree)
			BLI_bvhtree_free(collmd->bvhtree);
		if(collmd->x)
			MEM_freeN(collmd->x);
		if(collmd->xnew)
			MEM_freeN(collmd->xnew);
		if(collmd->current_x)
			MEM_freeN(collmd->current_x);
		if(collmd->current_xnew)
			MEM_freeN(collmd->current_xnew);
		if(collmd->current_v)
			MEM_freeN(collmd->current_v);
		if(collmd->mfaces)
			MEM_freeN(collmd->mfaces);
		
		collmd->x = NULL;
		collmd->xnew = NULL;
		collmd->current_x = NULL;
		collmd->current_xnew = NULL;
		collmd->current_v = NULL;
		collmd->time = -1000;
		collmd->numverts = 0;
		collmd->bvhtree = NULL;
		collmd->mfaces = NULL;
	}
}

static int collisionModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static void collisionModifier_deformVerts(
					  ModifierData *md, Object *ob, DerivedMesh *derivedData,
       float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	CollisionModifierData *collmd = (CollisionModifierData*) md;
	DerivedMesh *dm = NULL;
	float current_time = 0;
	unsigned int numverts = 0, i = 0;
	MVert *tempVert = NULL;
	
	/* if possible use/create DerivedMesh */
	if(derivedData) dm = CDDM_copy(derivedData);
	else if(ob->type==OB_MESH) dm = CDDM_from_mesh(ob->data, ob);
	
	if(!ob->pd)
	{
		printf("collisionModifier_deformVerts: Should not happen!\n");
		return;
	}
	
	if(dm)
	{
		CDDM_apply_vert_coords(dm, vertexCos);
		CDDM_calc_normals(dm);
		
		current_time = bsystem_time (md->scene,  ob, ( float ) md->scene->r.cfra, 0.0 );
		
		if(G.rt > 0)
			printf("current_time %f, collmd->time %f\n", current_time, collmd->time);
		
		numverts = dm->getNumVerts ( dm );
		
		if((current_time > collmd->time)|| (BKE_ptcache_get_continue_physics()))
		{	
			// check if mesh has changed
			if(collmd->x && (numverts != collmd->numverts))
				collisionModifier_freeData((ModifierData *)collmd);
			
			if(collmd->time == -1000) // first time
			{
				collmd->x = dm->dupVertArray(dm); // frame start position
				
				for ( i = 0; i < numverts; i++ )
				{
					// we save global positions
					mul_m4_v3( ob->obmat, collmd->x[i].co );
				}
				
				collmd->xnew = MEM_dupallocN(collmd->x); // frame end position
				collmd->current_x = MEM_dupallocN(collmd->x); // inter-frame
				collmd->current_xnew = MEM_dupallocN(collmd->x); // inter-frame
				collmd->current_v = MEM_dupallocN(collmd->x); // inter-frame

				collmd->numverts = numverts;
				
				collmd->mfaces = dm->dupFaceArray(dm);
				collmd->numfaces = dm->getNumFaces(dm);
				
				// create bounding box hierarchy
				collmd->bvhtree = bvhtree_build_from_mvert(collmd->mfaces, collmd->numfaces, collmd->x, numverts, ob->pd->pdef_sboft);
				
				collmd->time = current_time;
			}
			else if(numverts == collmd->numverts)
			{
				// put positions to old positions
				tempVert = collmd->x;
				collmd->x = collmd->xnew;
				collmd->xnew = tempVert;
				
				memcpy(collmd->xnew, dm->getVertArray(dm), numverts*sizeof(MVert));
				
				for ( i = 0; i < numverts; i++ )
				{
					// we save global positions
					mul_m4_v3( ob->obmat, collmd->xnew[i].co );
				}
				
				memcpy(collmd->current_xnew, collmd->x, numverts*sizeof(MVert));
				memcpy(collmd->current_x, collmd->x, numverts*sizeof(MVert));
				
				/* check if GUI setting has changed for bvh */
				if(collmd->bvhtree) 
				{
					if(ob->pd->pdef_sboft != BLI_bvhtree_getepsilon(collmd->bvhtree))
					{
						BLI_bvhtree_free(collmd->bvhtree);
						collmd->bvhtree = bvhtree_build_from_mvert(collmd->mfaces, collmd->numfaces, collmd->current_x, numverts, ob->pd->pdef_sboft);
					}
			
				}
				
				/* happens on file load (ONLY when i decomment changes in readfile.c) */
				if(!collmd->bvhtree)
				{
					collmd->bvhtree = bvhtree_build_from_mvert(collmd->mfaces, collmd->numfaces, collmd->current_x, numverts, ob->pd->pdef_sboft);
				}
				else
				{
					// recalc static bounding boxes
					bvhtree_update_from_mvert ( collmd->bvhtree, collmd->mfaces, collmd->numfaces, collmd->current_x, collmd->current_xnew, collmd->numverts, 1 );
				}
				
				collmd->time = current_time;
			}
			else if(numverts != collmd->numverts)
			{
				collisionModifier_freeData((ModifierData *)collmd);
			}
			
		}
		else if(current_time < collmd->time)
		{	
			collisionModifier_freeData((ModifierData *)collmd);
		}
		else
		{
			if(numverts != collmd->numverts)
			{
				collisionModifier_freeData((ModifierData *)collmd);
			}
		}
	}
	
	if(dm)
		dm->release(dm);
}



/* Surface */

static void surfaceModifier_initData(ModifierData *md) 
{
	SurfaceModifierData *surmd = (SurfaceModifierData*) md;
	
	surmd->bvhtree = NULL;
}

static void surfaceModifier_freeData(ModifierData *md)
{
	SurfaceModifierData *surmd = (SurfaceModifierData*) md;
	
	if (surmd)
	{
		if(surmd->bvhtree) {
			free_bvhtree_from_mesh(surmd->bvhtree);
			MEM_freeN(surmd->bvhtree);
		}

		if(surmd->dm)
			surmd->dm->release(surmd->dm);

		if(surmd->x)
			MEM_freeN(surmd->x);
		
		if(surmd->v)
			MEM_freeN(surmd->v);

		surmd->bvhtree = NULL;
		surmd->dm = NULL;
	}
}

static int surfaceModifier_dependsOnTime(ModifierData *md)
{
	return 1;
}

static void surfaceModifier_deformVerts(
					  ModifierData *md, Object *ob, DerivedMesh *derivedData,
	    float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	SurfaceModifierData *surmd = (SurfaceModifierData*) md;
	unsigned int numverts = 0, i = 0;
	
	if(surmd->dm)
		surmd->dm->release(surmd->dm);

	/* if possible use/create DerivedMesh */
	if(derivedData) surmd->dm = CDDM_copy(derivedData);
	else surmd->dm = get_dm(md->scene, ob, NULL, NULL, NULL, 0);
	
	if(!ob->pd)
	{
		printf("surfaceModifier_deformVerts: Should not happen!\n");
		return;
	}
	
	if(surmd->dm)
	{
		int init = 0;
		float *vec;
		MVert *x, *v;

		CDDM_apply_vert_coords(surmd->dm, vertexCos);
		CDDM_calc_normals(surmd->dm);
		
		numverts = surmd->dm->getNumVerts ( surmd->dm );

		if(numverts != surmd->numverts || surmd->x == NULL || surmd->v == NULL || md->scene->r.cfra != surmd->cfra+1) {
			if(surmd->x) {
				MEM_freeN(surmd->x);
				surmd->x = NULL;
			}
			if(surmd->v) {
				MEM_freeN(surmd->v);
				surmd->v = NULL;
			}

			surmd->x = MEM_callocN(numverts * sizeof(MVert), "MVert");
			surmd->v = MEM_callocN(numverts * sizeof(MVert), "MVert");

			surmd->numverts = numverts;

			init = 1;
		}

		/* convert to global coordinates and calculate velocity */
		for(i = 0, x = surmd->x, v = surmd->v; i<numverts; i++, x++, v++) {
			vec = CDDM_get_vert(surmd->dm, i)->co;
			mul_m4_v3(ob->obmat, vec);

			if(init)
				v->co[0] = v->co[1] = v->co[2] = 0.0f;
			else
				sub_v3_v3v3(v->co, vec, x->co);
			
			copy_v3_v3(x->co, vec);
		}

		surmd->cfra = md->scene->r.cfra;

		if(surmd->bvhtree)
			free_bvhtree_from_mesh(surmd->bvhtree);
		else
			surmd->bvhtree = MEM_callocN(sizeof(BVHTreeFromMesh), "BVHTreeFromMesh");

		if(surmd->dm->getNumFaces(surmd->dm))
			bvhtree_from_mesh_faces(surmd->bvhtree, surmd->dm, 0.0, 2, 6);
		else
			bvhtree_from_mesh_edges(surmd->bvhtree, surmd->dm, 0.0, 2, 6);
	}
}


/* Boolean */

static void booleanModifier_copyData(ModifierData *md, ModifierData *target)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;
	BooleanModifierData *tbmd = (BooleanModifierData*) target;

	tbmd->object = bmd->object;
	tbmd->operation = bmd->operation;
}

static int booleanModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;

	return !bmd->object;
}

static void booleanModifier_foreachObjectLink(
					      ModifierData *md, Object *ob,
	   void (*walk)(void *userData, Object *ob, Object **obpoin),
		  void *userData)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;

	walk(userData, ob, &bmd->object);
}

static void booleanModifier_updateDepgraph(
					   ModifierData *md, DagForest *forest, Scene *scene, Object *ob,
	DagNode *obNode)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;

	if(bmd->object) {
		DagNode *curNode = dag_get_node(forest, bmd->object);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Boolean Modifier");
	}
}

static DerivedMesh *booleanModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	// XXX doesn't handle derived data
	BooleanModifierData *bmd = (BooleanModifierData*) md;
	DerivedMesh *dm = bmd->object->derivedFinal;

	/* we do a quick sanity check */
	if(dm && (derivedData->getNumFaces(derivedData) > 3)
		    && bmd->object && dm->getNumFaces(dm) > 3) {
		DerivedMesh *result = NewBooleanDerivedMesh(dm, bmd->object, derivedData, ob,
				1 + bmd->operation);

		if(dm)
			dm->release(dm);

		/* if new mesh returned, return it; otherwise there was
		* an error, so delete the modifier object */
		if(result)
			return result;
		else
			bmd->object = NULL;
	}
	
	if(dm)
			dm->release(dm);

	return derivedData;
}

static CustomDataMask booleanModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	CustomDataMask dataMask = (1 << CD_MTFACE) + (1 << CD_MEDGE);

	dataMask |= (1 << CD_MDEFORMVERT);
	
	return dataMask;
}

/* Particles */
static void particleSystemModifier_initData(ModifierData *md) 
{
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;
	psmd->psys= 0;
	psmd->dm=0;
	psmd->totdmvert= psmd->totdmedge= psmd->totdmface= 0;
}
static void particleSystemModifier_freeData(ModifierData *md)
{
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;

	if(psmd->dm){
		psmd->dm->needsFree = 1;
		psmd->dm->release(psmd->dm);
		psmd->dm=0;
	}

	/* ED_object_modifier_remove may have freed this first before calling
	 * modifier_free (which calls this function) */
	if(psmd->psys)
		psmd->psys->flag |= PSYS_DELETE;
}
static void particleSystemModifier_copyData(ModifierData *md, ModifierData *target)
{
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;
	ParticleSystemModifierData *tpsmd= (ParticleSystemModifierData*) target;

	tpsmd->dm = 0;
	tpsmd->totdmvert = tpsmd->totdmedge = tpsmd->totdmface = 0;
	//tpsmd->facepa = 0;
	tpsmd->flag = psmd->flag;
	/* need to keep this to recognise a bit later in copy_object */
	tpsmd->psys = psmd->psys;
}

static CustomDataMask particleSystemModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;
	CustomDataMask dataMask = 0;
	Material *ma;
	MTex *mtex;
	int i;

	if(!psmd->psys->part)
		return 0;

	ma= give_current_material(ob, psmd->psys->part->omat);
	if(ma) {
		for(i=0; i<MAX_MTEX; i++) {
			mtex=ma->mtex[i];
			if(mtex && (ma->septex & (1<<i))==0)
				if(mtex->pmapto && (mtex->texco & TEXCO_UV))
					dataMask |= (1 << CD_MTFACE);
		}
	}

	if(psmd->psys->part->tanfac!=0.0)
		dataMask |= (1 << CD_MTFACE);

	/* ask for vertexgroups if we need them */
	for(i=0; i<PSYS_TOT_VG; i++){
		if(psmd->psys->vgroup[i]){
			dataMask |= (1 << CD_MDEFORMVERT);
			break;
		}
	}
	
	/* particles only need this if they are after a non deform modifier, and
	* the modifier stack will only create them in that case. */
	dataMask |= CD_MASK_ORIGSPACE;

	dataMask |= CD_MASK_ORCO;
	
	return dataMask;
}

/* saves the current emitter state for a particle system and calculates particles */
static void particleSystemModifier_deformVerts(
					       ModifierData *md, Object *ob, DerivedMesh *derivedData,
	    float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	ParticleSystemModifierData *psmd= (ParticleSystemModifierData*) md;
	ParticleSystem * psys=0;
	int needsFree=0;

	if(ob->particlesystem.first)
		psys=psmd->psys;
	else
		return;
	
	if(!psys_check_enabled(ob, psys))
		return;

	if(dm==0) {
		dm= get_dm(md->scene, ob, NULL, NULL, vertexCos, 1);

		if(!dm)
			return;

		needsFree= 1;
	}

	/* clear old dm */
	if(psmd->dm){
		psmd->dm->needsFree = 1;
		psmd->dm->release(psmd->dm);
	}

	/* make new dm */
	psmd->dm=CDDM_copy(dm);
	CDDM_apply_vert_coords(psmd->dm, vertexCos);
	CDDM_calc_normals(psmd->dm);

	if(needsFree){
		dm->needsFree = 1;
		dm->release(dm);
	}

	/* protect dm */
	psmd->dm->needsFree = 0;

	/* report change in mesh structure */
	if(psmd->dm->getNumVerts(psmd->dm)!=psmd->totdmvert ||
		  psmd->dm->getNumEdges(psmd->dm)!=psmd->totdmedge ||
		  psmd->dm->getNumFaces(psmd->dm)!=psmd->totdmface){
		/* in file read dm hasn't really changed but just wasn't saved in file */

		psys->recalc |= PSYS_RECALC_RESET;
		psmd->flag |= eParticleSystemFlag_DM_changed;

		psmd->totdmvert= psmd->dm->getNumVerts(psmd->dm);
		psmd->totdmedge= psmd->dm->getNumEdges(psmd->dm);
		psmd->totdmface= psmd->dm->getNumFaces(psmd->dm);
		  }

		  if(psys){
			  psmd->flag &= ~eParticleSystemFlag_psys_updated;
			  particle_system_update(md->scene, ob, psys);
			  psmd->flag |= eParticleSystemFlag_psys_updated;
			  psmd->flag &= ~eParticleSystemFlag_DM_changed;
		  }
}

/* disabled particles in editmode for now, until support for proper derivedmesh
 * updates is coded */
#if 0
static void particleSystemModifier_deformVertsEM(
                ModifierData *md, Object *ob, EditMesh *editData,
                DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	particleSystemModifier_deformVerts(md, ob, dm, vertexCos, numVerts);

	if(!derivedData) dm->release(dm);
}
#endif

/* Particle Instance */
static void particleInstanceModifier_initData(ModifierData *md) 
{
	ParticleInstanceModifierData *pimd= (ParticleInstanceModifierData*) md;

	pimd->flag = eParticleInstanceFlag_Parents|eParticleInstanceFlag_Unborn|
			eParticleInstanceFlag_Alive|eParticleInstanceFlag_Dead;
	pimd->psys = 1;
	pimd->position = 1.0f;
	pimd->axis = 2;

}
static void particleInstanceModifier_copyData(ModifierData *md, ModifierData *target)
{
	ParticleInstanceModifierData *pimd= (ParticleInstanceModifierData*) md;
	ParticleInstanceModifierData *tpimd= (ParticleInstanceModifierData*) target;

	tpimd->ob = pimd->ob;
	tpimd->psys = pimd->psys;
	tpimd->flag = pimd->flag;
	tpimd->axis = pimd->axis;
	tpimd->position = pimd->position;
	tpimd->random_position = pimd->random_position;
}

static int particleInstanceModifier_dependsOnTime(ModifierData *md) 
{
	return 0;
}
static void particleInstanceModifier_updateDepgraph(ModifierData *md, DagForest *forest,
		 Scene *scene,Object *ob, DagNode *obNode)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData*) md;

	if (pimd->ob) {
		DagNode *curNode = dag_get_node(forest, pimd->ob);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
				 "Particle Instance Modifier");
	}
}

static void particleInstanceModifier_foreachObjectLink(ModifierData *md, Object *ob,
		ObjectWalkFunc walk, void *userData)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData*) md;

	walk(userData, ob, &pimd->ob);
}

static DerivedMesh * particleInstanceModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData, *result;
	ParticleInstanceModifierData *pimd= (ParticleInstanceModifierData*) md;
	ParticleSimulationData sim;
	ParticleSystem * psys=0;
	ParticleData *pa=0, *pars=0;
	MFace *mface, *orig_mface;
	MVert *mvert, *orig_mvert;
	int i,totvert, totpart=0, totface, maxvert, maxface, first_particle=0;
	short track=ob->trackflag%3, trackneg, axis = pimd->axis;
	float max_co=0.0, min_co=0.0, temp_co[3], cross[3];
	float *size=NULL;

	trackneg=((ob->trackflag>2)?1:0);

	if(pimd->ob==ob){
		pimd->ob=0;
		return derivedData;
	}

	if(pimd->ob){
		psys = BLI_findlink(&pimd->ob->particlesystem,pimd->psys-1);
		if(psys==0 || psys->totpart==0)
			return derivedData;
	}
	else return derivedData;

	if(pimd->flag & eParticleInstanceFlag_Parents)
		totpart+=psys->totpart;
	if(pimd->flag & eParticleInstanceFlag_Children){
		if(totpart==0)
			first_particle=psys->totpart;
		totpart+=psys->totchild;
	}

	if(totpart==0)
		return derivedData;

	sim.scene = md->scene;
	sim.ob = pimd->ob;
	sim.psys = psys;
	sim.psmd = psys_get_modifier(pimd->ob, psys);

	if(pimd->flag & eParticleInstanceFlag_UseSize) {
		int p;
		float *si;
		si = size = MEM_callocN(totpart * sizeof(float), "particle size array");

		if(pimd->flag & eParticleInstanceFlag_Parents) {
			for(p=0, pa= psys->particles; p<psys->totpart; p++, pa++, si++)
				*si = pa->size;
		}

		if(pimd->flag & eParticleInstanceFlag_Children) {
			ChildParticle *cpa = psys->child;

			for(p=0; p<psys->totchild; p++, cpa++, si++) {
				*si = psys_get_child_size(psys, cpa, 0.0f, NULL);
			}
		}
	}

	pars=psys->particles;

	totvert=dm->getNumVerts(dm);
	totface=dm->getNumFaces(dm);

	maxvert=totvert*totpart;
	maxface=totface*totpart;

	psys->lattice=psys_get_lattice(&sim);

	if(psys->flag & (PSYS_HAIR_DONE|PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED){

		float min_r[3], max_r[3];
		INIT_MINMAX(min_r, max_r);
		dm->getMinMax(dm, min_r, max_r);		
		min_co=min_r[track];
		max_co=max_r[track];
	}

	result = CDDM_from_template(dm, maxvert,dm->getNumEdges(dm)*totpart,maxface);

	mvert=result->getVertArray(result);
	orig_mvert=dm->getVertArray(dm);

	for(i=0; i<maxvert; i++){
		MVert *inMV;
		MVert *mv = mvert + i;
		ParticleKey state;

		inMV = orig_mvert + i%totvert;
		DM_copy_vert_data(dm, result, i%totvert, i, 1);
		*mv = *inMV;

		/*change orientation based on object trackflag*/
		VECCOPY(temp_co,mv->co);
		mv->co[axis]=temp_co[track];
		mv->co[(axis+1)%3]=temp_co[(track+1)%3];
		mv->co[(axis+2)%3]=temp_co[(track+2)%3];

		if((psys->flag & (PSYS_HAIR_DONE|PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED) && pimd->flag & eParticleInstanceFlag_Path){
			float ran = 0.0f;
			if(pimd->random_position != 0.0f) {
				BLI_srandom(psys->seed + (i/totvert)%totpart);
				ran = pimd->random_position * BLI_frand();
			}

			if(pimd->flag & eParticleInstanceFlag_KeepShape) {
				state.time = pimd->position * (1.0f - ran);
			}
			else {
				state.time=(mv->co[axis]-min_co)/(max_co-min_co) * pimd->position * (1.0f - ran);

				if(trackneg)
					state.time=1.0f-state.time;
				
				mv->co[axis] = 0.0;
			}

			psys_get_particle_on_path(&sim, first_particle + i/totvert, &state,1);

			normalize_v3(state.vel);
			
			/* TODO: incremental rotations somehow */
			if(state.vel[axis] < -0.9999 || state.vel[axis] > 0.9999) {
				state.rot[0] = 1;
				state.rot[1] = state.rot[2] = state.rot[3] = 0.0f;
			}
			else {
				float temp[3] = {0.0f,0.0f,0.0f};
				temp[axis] = 1.0f;

				cross_v3_v3v3(cross, temp, state.vel);

				/* state.vel[axis] is the only component surviving from a dot product with the axis */
				axis_angle_to_quat(state.rot,cross,saacos(state.vel[axis]));
			}

		}
		else{
			state.time=-1.0;
			psys_get_particle_state(&sim, first_particle + i/totvert, &state,1);
		}	

		mul_qt_v3(state.rot,mv->co);
		if(pimd->flag & eParticleInstanceFlag_UseSize)
			mul_v3_fl(mv->co, size[i/totvert]);
		VECADD(mv->co,mv->co,state.co);
	}

	mface=result->getFaceArray(result);
	orig_mface=dm->getFaceArray(dm);

	for(i=0; i<maxface; i++){
		MFace *inMF;
		MFace *mf = mface + i;

		if(pimd->flag & eParticleInstanceFlag_Parents){
			if(i/totface>=psys->totpart){
				if(psys->part->childtype==PART_CHILD_PARTICLES)
					pa=psys->particles+(psys->child+i/totface-psys->totpart)->parent;
				else
					pa=0;
			}
			else
				pa=pars+i/totface;
		}
		else{
			if(psys->part->childtype==PART_CHILD_PARTICLES)
				pa=psys->particles+(psys->child+i/totface)->parent;
			else
				pa=0;
		}

		if(pa){
			if(pa->alive==PARS_UNBORN && (pimd->flag&eParticleInstanceFlag_Unborn)==0) continue;
			if(pa->alive==PARS_ALIVE && (pimd->flag&eParticleInstanceFlag_Alive)==0) continue;
			if(pa->alive==PARS_DEAD && (pimd->flag&eParticleInstanceFlag_Dead)==0) continue;
		}

		inMF = orig_mface + i%totface;
		DM_copy_face_data(dm, result, i%totface, i, 1);
		*mf = *inMF;

		mf->v1+=(i/totface)*totvert;
		mf->v2+=(i/totface)*totvert;
		mf->v3+=(i/totface)*totvert;
		if(mf->v4)
			mf->v4+=(i/totface)*totvert;
	}
	
	CDDM_calc_edges(result);
	CDDM_calc_normals(result);

	if(psys->lattice){
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}

	if(size)
		MEM_freeN(size);

	return result;
}
static DerivedMesh *particleInstanceModifier_applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return particleInstanceModifier_applyModifier(md, ob, derivedData, 0, 1);
}

/* Explode */
static void explodeModifier_initData(ModifierData *md)
{
	ExplodeModifierData *emd= (ExplodeModifierData*) md;

	emd->facepa=0;
	emd->flag |= eExplodeFlag_Unborn+eExplodeFlag_Alive+eExplodeFlag_Dead;
}
static void explodeModifier_freeData(ModifierData *md)
{
	ExplodeModifierData *emd= (ExplodeModifierData*) md;
	
	if(emd->facepa) MEM_freeN(emd->facepa);
}
static void explodeModifier_copyData(ModifierData *md, ModifierData *target)
{
	ExplodeModifierData *emd= (ExplodeModifierData*) md;
	ExplodeModifierData *temd= (ExplodeModifierData*) target;

	temd->facepa = 0;
	temd->flag = emd->flag;
	temd->protect = emd->protect;
	temd->vgroup = emd->vgroup;
}
static int explodeModifier_dependsOnTime(ModifierData *md) 
{
	return 1;
}
static CustomDataMask explodeModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	ExplodeModifierData *emd= (ExplodeModifierData*) md;
	CustomDataMask dataMask = 0;

	if(emd->vgroup)
		dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void explodeModifier_createFacepa(ExplodeModifierData *emd,
					 ParticleSystemModifierData *psmd,
      Object *ob, DerivedMesh *dm)
{
	ParticleSystem *psys=psmd->psys;
	MFace *fa=0, *mface=0;
	MVert *mvert = 0;
	ParticleData *pa;
	KDTree *tree;
	float center[3], co[3];
	int *facepa=0,*vertpa=0,totvert=0,totface=0,totpart=0;
	int i,p,v1,v2,v3,v4=0;

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);
	totface= dm->getNumFaces(dm);
	totvert= dm->getNumVerts(dm);
	totpart= psmd->psys->totpart;

	BLI_srandom(psys->seed);

	if(emd->facepa)
		MEM_freeN(emd->facepa);

	facepa = emd->facepa = MEM_callocN(sizeof(int)*totface, "explode_facepa");

	vertpa = MEM_callocN(sizeof(int)*totvert, "explode_vertpa");

	/* initialize all faces & verts to no particle */
	for(i=0; i<totface; i++)
		facepa[i]=totpart;

	for (i=0; i<totvert; i++)
		vertpa[i]=totpart;

	/* set protected verts */
	if(emd->vgroup){
		MDeformVert *dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
		float val;
		if(dvert){
			for(i=0; i<totvert; i++){
				val = BLI_frand();
				val = (1.0f-emd->protect)*val + emd->protect*0.5f;
				if(val < deformvert_get_weight(dvert+i,emd->vgroup-1))
					vertpa[i] = -1;
			}
		}
	}

	/* make tree of emitter locations */
	tree=BLI_kdtree_new(totpart);
	for(p=0,pa=psys->particles; p<totpart; p++,pa++){
		psys_particle_on_dm(psmd->dm,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,0,0,0,0,0);
		BLI_kdtree_insert(tree, p, co, NULL);
	}
	BLI_kdtree_balance(tree);

	/* set face-particle-indexes to nearest particle to face center */
	for(i=0,fa=mface; i<totface; i++,fa++){
		add_v3_v3v3(center,mvert[fa->v1].co,mvert[fa->v2].co);
		add_v3_v3v3(center,center,mvert[fa->v3].co);
		if(fa->v4){
			add_v3_v3v3(center,center,mvert[fa->v4].co);
			mul_v3_fl(center,0.25);
		}
		else
			mul_v3_fl(center,0.3333f);

		p= BLI_kdtree_find_nearest(tree,center,NULL,NULL);

		v1=vertpa[fa->v1];
		v2=vertpa[fa->v2];
		v3=vertpa[fa->v3];
		if(fa->v4)
			v4=vertpa[fa->v4];

		if(v1>=0 && v2>=0 && v3>=0 && (fa->v4==0 || v4>=0))
			facepa[i]=p;

		if(v1>=0) vertpa[fa->v1]=p;
		if(v2>=0) vertpa[fa->v2]=p;
		if(v3>=0) vertpa[fa->v3]=p;
		if(fa->v4 && v4>=0) vertpa[fa->v4]=p;
	}

	if(vertpa) MEM_freeN(vertpa);
	BLI_kdtree_free(tree);
}

static int edgesplit_get(EdgeHash *edgehash, int v1, int v2)
{
	return GET_INT_FROM_POINTER(BLI_edgehash_lookup(edgehash, v1, v2));
}

static DerivedMesh * explodeModifier_splitEdges(ExplodeModifierData *emd, DerivedMesh *dm){
	DerivedMesh *splitdm;
	MFace *mf=0,*df1=0,*df2=0,*df3=0;
	MFace *mface=CDDM_get_faces(dm);
	MVert *dupve, *mv;
	EdgeHash *edgehash;
	EdgeHashIterator *ehi;
	int totvert=dm->getNumVerts(dm);
	int totface=dm->getNumFaces(dm);

	int *facesplit = MEM_callocN(sizeof(int)*totface,"explode_facesplit");
	int *vertpa = MEM_callocN(sizeof(int)*totvert,"explode_vertpa2");
	int *facepa = emd->facepa;
	int *fs, totesplit=0,totfsplit=0,totin=0,curdupvert=0,curdupface=0,curdupin=0;
	int i,j,v1,v2,v3,v4,esplit;

	edgehash= BLI_edgehash_new();

	/* recreate vertpa from facepa calculation */
	for (i=0,mf=mface; i<totface; i++,mf++) {
		vertpa[mf->v1]=facepa[i];
		vertpa[mf->v2]=facepa[i];
		vertpa[mf->v3]=facepa[i];
		if(mf->v4)
			vertpa[mf->v4]=facepa[i];
	}

	/* mark edges for splitting and how to split faces */
	for (i=0,mf=mface,fs=facesplit; i<totface; i++,mf++,fs++) {
		if(mf->v4){
			v1=vertpa[mf->v1];
			v2=vertpa[mf->v2];
			v3=vertpa[mf->v3];
			v4=vertpa[mf->v4];

			if(v1!=v2){
				BLI_edgehash_insert(edgehash, mf->v1, mf->v2, NULL);
				(*fs)++;
			}

			if(v2!=v3){
				BLI_edgehash_insert(edgehash, mf->v2, mf->v3, NULL);
				(*fs)++;
			}

			if(v3!=v4){
				BLI_edgehash_insert(edgehash, mf->v3, mf->v4, NULL);
				(*fs)++;
			}

			if(v1!=v4){
				BLI_edgehash_insert(edgehash, mf->v1, mf->v4, NULL);
				(*fs)++;
			}

			if(*fs==2){
				if((v1==v2 && v3==v4) || (v1==v4 && v2==v3))
					*fs=1;
				else if(v1!=v2){
					if(v1!=v4)
						BLI_edgehash_insert(edgehash, mf->v2, mf->v3, NULL);
					else
						BLI_edgehash_insert(edgehash, mf->v3, mf->v4, NULL);
				}
				else{ 
					if(v1!=v4)
						BLI_edgehash_insert(edgehash, mf->v1, mf->v2, NULL);
					else
						BLI_edgehash_insert(edgehash, mf->v1, mf->v4, NULL);
				}
			}
		}
	}

	/* count splits & reindex */
	ehi= BLI_edgehashIterator_new(edgehash);
	totesplit=totvert;
	for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		BLI_edgehashIterator_setValue(ehi, SET_INT_IN_POINTER(totesplit));
		totesplit++;
	}
	BLI_edgehashIterator_free(ehi);

	/* count new faces due to splitting */
	for(i=0,fs=facesplit; i<totface; i++,fs++){
		if(*fs==1)
			totfsplit+=1;
		else if(*fs==2)
			totfsplit+=2;
		else if(*fs==3)
			totfsplit+=3;
		else if(*fs==4){
			totfsplit+=3;

			mf=dm->getFaceData(dm,i,CD_MFACE);//CDDM_get_face(dm,i);

			if(vertpa[mf->v1]!=vertpa[mf->v2] && vertpa[mf->v2]!=vertpa[mf->v3])
				totin++;
		}
	}
	
	splitdm= CDDM_from_template(dm, totesplit+totin, dm->getNumEdges(dm),totface+totfsplit);

	/* copy new faces & verts (is it really this painful with custom data??) */
	for(i=0; i<totvert; i++){
		MVert source;
		MVert *dest;
		dm->getVert(dm, i, &source);
		dest = CDDM_get_vert(splitdm, i);

		DM_copy_vert_data(dm, splitdm, i, i, 1);
		*dest = source;
	}
	for(i=0; i<totface; i++){
		MFace source;
		MFace *dest;
		dm->getFace(dm, i, &source);
		dest = CDDM_get_face(splitdm, i);

		DM_copy_face_data(dm, splitdm, i, i, 1);
		*dest = source;
	}

	/* override original facepa (original pointer is saved in caller function) */
	facepa= MEM_callocN(sizeof(int)*(totface+totfsplit),"explode_facepa");
	memcpy(facepa,emd->facepa,totface*sizeof(int));
	emd->facepa=facepa;

	/* create new verts */
	curdupvert=totvert;
	ehi= BLI_edgehashIterator_new(edgehash);
	for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		BLI_edgehashIterator_getKey(ehi, &i, &j);
		esplit= GET_INT_FROM_POINTER(BLI_edgehashIterator_getValue(ehi));
		mv=CDDM_get_vert(splitdm,j);
		dupve=CDDM_get_vert(splitdm,esplit);

		DM_copy_vert_data(splitdm,splitdm,j,esplit,1);

		*dupve=*mv;

		mv=CDDM_get_vert(splitdm,i);

		VECADD(dupve->co,dupve->co,mv->co);
		mul_v3_fl(dupve->co,0.5);
	}
	BLI_edgehashIterator_free(ehi);

	/* create new faces */
	curdupface=totface;
	curdupin=totesplit;
	for(i=0,fs=facesplit; i<totface; i++,fs++){
		if(*fs){
			mf=CDDM_get_face(splitdm,i);

			v1=vertpa[mf->v1];
			v2=vertpa[mf->v2];
			v3=vertpa[mf->v3];
			v4=vertpa[mf->v4];
			/* ouch! creating new faces & remapping them to new verts is no fun */
			if(*fs==1){
				df1=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df1=*mf;
				curdupface++;
				
				if(v1==v2){
					df1->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
					df1->v2=edgesplit_get(edgehash, mf->v2, mf->v3);
					mf->v3=df1->v2;
					mf->v4=df1->v1;
				}
				else{
					df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
					df1->v4=edgesplit_get(edgehash, mf->v3, mf->v4);
					mf->v2=df1->v1;
					mf->v3=df1->v4;
				}

				facepa[i]=v1;
				facepa[curdupface-1]=v3;

				test_index_face(df1, &splitdm->faceData, curdupface, (df1->v4 ? 4 : 3));
			}
			if(*fs==2){
				df1=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df1=*mf;
				curdupface++;

				df2=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df2=*mf;
				curdupface++;

				if(v1!=v2){
					if(v1!=v4){
						df1->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
						df1->v2=edgesplit_get(edgehash, mf->v1, mf->v2);
						df2->v1=df1->v3=mf->v2;
						df2->v3=df1->v4=mf->v4;
						df2->v2=mf->v3;

						mf->v2=df1->v2;
						mf->v3=df1->v1;

						df2->v4=mf->v4=0;

						facepa[i]=v1;
					}
					else{
						df1->v2=edgesplit_get(edgehash, mf->v1, mf->v2);
						df1->v3=edgesplit_get(edgehash, mf->v2, mf->v3);
						df1->v4=mf->v3;
						df2->v2=mf->v3;
						df2->v3=mf->v4;

						mf->v1=df1->v2;
						mf->v3=df1->v3;

						df2->v4=mf->v4=0;

						facepa[i]=v2;
					}
					facepa[curdupface-1]=facepa[curdupface-2]=v3;
				}
				else{
					if(v1!=v4){
						df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);
						df1->v4=edgesplit_get(edgehash, mf->v1, mf->v4);
						df1->v2=mf->v3;

						mf->v1=df1->v4;
						mf->v2=df1->v3;
						mf->v3=mf->v4;

						df2->v4=mf->v4=0;

						facepa[i]=v4;
					}
					else{
						df1->v3=edgesplit_get(edgehash, mf->v2, mf->v3);
						df1->v4=edgesplit_get(edgehash, mf->v3, mf->v4);
						df1->v1=mf->v4;
						df1->v2=mf->v2;
						df2->v3=mf->v4;

						mf->v1=df1->v4;
						mf->v2=df1->v3;

						df2->v4=mf->v4=0;

						facepa[i]=v3;
					}

					facepa[curdupface-1]=facepa[curdupface-2]=v1;
				}

				test_index_face(df1, &splitdm->faceData, curdupface-2, (df1->v4 ? 4 : 3));
				test_index_face(df1, &splitdm->faceData, curdupface-1, (df1->v4 ? 4 : 3));
			}
			else if(*fs==3){
				df1=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df1=*mf;
				curdupface++;

				df2=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df2=*mf;
				curdupface++;

				df3=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df3=*mf;
				curdupface++;

				if(v1==v2){
					df2->v1=df1->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
					df3->v1=df1->v2=edgesplit_get(edgehash, mf->v2, mf->v3);
					df3->v3=df2->v2=df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);
					df3->v2=mf->v3;
					df2->v3=mf->v4;
					df1->v4=df2->v4=df3->v4=0;

					mf->v3=df1->v2;
					mf->v4=df1->v1;

					facepa[i]=facepa[curdupface-3]=v1;
					facepa[curdupface-1]=v3;
					facepa[curdupface-2]=v4;
				}
				else if(v2==v3){
					df3->v1=df2->v3=df1->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
					df2->v2=df1->v2=edgesplit_get(edgehash, mf->v1, mf->v2);
					df3->v2=df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);

					df3->v3=mf->v4;
					df2->v1=mf->v1;
					df1->v4=df2->v4=df3->v4=0;

					mf->v1=df1->v2;
					mf->v4=df1->v3;

					facepa[i]=facepa[curdupface-3]=v2;
					facepa[curdupface-1]=v4;
					facepa[curdupface-2]=v1;
				}
				else if(v3==v4){
					df3->v2=df2->v1=df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
					df2->v3=df1->v2=edgesplit_get(edgehash, mf->v2, mf->v3);
					df3->v3=df1->v3=edgesplit_get(edgehash, mf->v1, mf->v4);

					df3->v1=mf->v1;
					df2->v2=mf->v2;
					df1->v4=df2->v4=df3->v4=0;

					mf->v1=df1->v3;
					mf->v2=df1->v2;

					facepa[i]=facepa[curdupface-3]=v3;
					facepa[curdupface-1]=v1;
					facepa[curdupface-2]=v2;
				}
				else{
					df3->v1=df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
					df3->v3=df2->v1=df1->v2=edgesplit_get(edgehash, mf->v2, mf->v3);
					df2->v3=df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);

					df3->v2=mf->v2;
					df2->v2=mf->v3;
					df1->v4=df2->v4=df3->v4=0;

					mf->v2=df1->v1;
					mf->v3=df1->v3;

					facepa[i]=facepa[curdupface-3]=v1;
					facepa[curdupface-1]=v2;
					facepa[curdupface-2]=v3;
				}

				test_index_face(df1, &splitdm->faceData, curdupface-3, (df1->v4 ? 4 : 3));
				test_index_face(df1, &splitdm->faceData, curdupface-2, (df1->v4 ? 4 : 3));
				test_index_face(df1, &splitdm->faceData, curdupface-1, (df1->v4 ? 4 : 3));
			}
			else if(*fs==4){
				if(v1!=v2 && v2!=v3){

					/* set new vert to face center */
					mv=CDDM_get_vert(splitdm,mf->v1);
					dupve=CDDM_get_vert(splitdm,curdupin);
					DM_copy_vert_data(splitdm,splitdm,mf->v1,curdupin,1);
					*dupve=*mv;

					mv=CDDM_get_vert(splitdm,mf->v2);
					VECADD(dupve->co,dupve->co,mv->co);
					mv=CDDM_get_vert(splitdm,mf->v3);
					VECADD(dupve->co,dupve->co,mv->co);
					mv=CDDM_get_vert(splitdm,mf->v4);
					VECADD(dupve->co,dupve->co,mv->co);
					mul_v3_fl(dupve->co,0.25);


					df1=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df1=*mf;
					curdupface++;

					df2=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df2=*mf;
					curdupface++;

					df3=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df3=*mf;
					curdupface++;

					df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
					df3->v2=df1->v3=edgesplit_get(edgehash, mf->v2, mf->v3);

					df2->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
					df3->v4=df2->v3=edgesplit_get(edgehash, mf->v3, mf->v4);

					df3->v1=df2->v2=df1->v4=curdupin;

					mf->v2=df1->v1;
					mf->v3=curdupin;
					mf->v4=df2->v1;

					curdupin++;

					facepa[i]=v1;
					facepa[curdupface-3]=v2;
					facepa[curdupface-2]=v3;
					facepa[curdupface-1]=v4;

					test_index_face(df1, &splitdm->faceData, curdupface-3, (df1->v4 ? 4 : 3));

					test_index_face(df1, &splitdm->faceData, curdupface-2, (df1->v4 ? 4 : 3));
					test_index_face(df1, &splitdm->faceData, curdupface-1, (df1->v4 ? 4 : 3));
				}
				else{
					df1=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df1=*mf;
					curdupface++;

					df2=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df2=*mf;
					curdupface++;

					df3=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df3=*mf;
					curdupface++;

					if(v2==v3){
						df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
						df3->v1=df1->v2=df1->v3=edgesplit_get(edgehash, mf->v2, mf->v3);
						df2->v1=df1->v4=edgesplit_get(edgehash, mf->v1, mf->v4);

						df3->v3=df2->v3=edgesplit_get(edgehash, mf->v3, mf->v4);

						df3->v2=mf->v3;
						df3->v4=0;

						mf->v2=df1->v1;
						mf->v3=df1->v4;
						mf->v4=0;

						facepa[i]=v1;
						facepa[curdupface-3]=facepa[curdupface-2]=v2;
						facepa[curdupface-1]=v3;
					}
					else{
						df3->v1=df2->v1=df1->v2=edgesplit_get(edgehash, mf->v1, mf->v2);
						df2->v4=df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);
						df1->v4=edgesplit_get(edgehash, mf->v1, mf->v4);

						df3->v3=df2->v2=edgesplit_get(edgehash, mf->v2, mf->v3);

						df3->v4=0;

						mf->v1=df1->v4;
						mf->v2=df1->v3;
						mf->v3=mf->v4;
						mf->v4=0;

						facepa[i]=v4;
						facepa[curdupface-3]=facepa[curdupface-2]=v1;
						facepa[curdupface-1]=v2;
					}

					test_index_face(df1, &splitdm->faceData, curdupface-3, (df1->v4 ? 4 : 3));
					test_index_face(df1, &splitdm->faceData, curdupface-2, (df1->v4 ? 4 : 3));
					test_index_face(df1, &splitdm->faceData, curdupface-1, (df1->v4 ? 4 : 3));
				}
			}

			test_index_face(df1, &splitdm->faceData, i, (df1->v4 ? 4 : 3));
		}
	}

	BLI_edgehash_free(edgehash, NULL);
	MEM_freeN(facesplit);
	MEM_freeN(vertpa);

	return splitdm;

}
static DerivedMesh * explodeModifier_explodeMesh(ExplodeModifierData *emd, 
		ParticleSystemModifierData *psmd, Scene *scene, Object *ob, 
  DerivedMesh *to_explode)
{
	DerivedMesh *explode, *dm=to_explode;
	MFace *mf=0, *mface;
	ParticleSettings *part=psmd->psys->part;
	ParticleSimulationData sim = {scene, ob, psmd->psys, psmd};
	ParticleData *pa=NULL, *pars=psmd->psys->particles;
	ParticleKey state;
	EdgeHash *vertpahash;
	EdgeHashIterator *ehi;
	float *vertco=0, imat[4][4];
	float loc0[3], nor[3];
	float timestep, cfra;
	int *facepa=emd->facepa;
	int totdup=0,totvert=0,totface=0,totpart=0;
	int i, j, v, mindex=0;

	totface= dm->getNumFaces(dm);
	totvert= dm->getNumVerts(dm);
	mface= dm->getFaceArray(dm);
	totpart= psmd->psys->totpart;

	timestep= psys_get_timestep(&sim);

	//if(part->flag & PART_GLOB_TIME)
		cfra=bsystem_time(scene, 0,(float)scene->r.cfra,0.0);
	//else
	//	cfra=bsystem_time(scene, ob,(float)scene->r.cfra,0.0);

	/* hash table for vertice <-> particle relations */
	vertpahash= BLI_edgehash_new();

	for (i=0; i<totface; i++) {
		/* do mindex + totvert to ensure the vertex index to be the first
		 * with BLI_edgehashIterator_getKey */
		if(facepa[i]==totpart || cfra <= (pars+facepa[i])->time)
			mindex = totvert+totpart;
		else 
			mindex = totvert+facepa[i];

		mf= &mface[i];

		/* set face vertices to exist in particle group */
		BLI_edgehash_insert(vertpahash, mf->v1, mindex, NULL);
		BLI_edgehash_insert(vertpahash, mf->v2, mindex, NULL);
		BLI_edgehash_insert(vertpahash, mf->v3, mindex, NULL);
		if(mf->v4)
			BLI_edgehash_insert(vertpahash, mf->v4, mindex, NULL);
	}

	/* make new vertice indexes & count total vertices after duplication */
	ehi= BLI_edgehashIterator_new(vertpahash);
	for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		BLI_edgehashIterator_setValue(ehi, SET_INT_IN_POINTER(totdup));
		totdup++;
	}
	BLI_edgehashIterator_free(ehi);

	/* the final duplicated vertices */
	explode= CDDM_from_template(dm, totdup, 0,totface);
	/*dupvert= CDDM_get_verts(explode);*/

	/* getting back to object space */
	invert_m4_m4(imat,ob->obmat);

	psmd->psys->lattice = psys_get_lattice(&sim);

	/* duplicate & displace vertices */
	ehi= BLI_edgehashIterator_new(vertpahash);
	for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		MVert source;
		MVert *dest;

		/* get particle + vertex from hash */
		BLI_edgehashIterator_getKey(ehi, &j, &i);
		i -= totvert;
		v= GET_INT_FROM_POINTER(BLI_edgehashIterator_getValue(ehi));

		dm->getVert(dm, j, &source);
		dest = CDDM_get_vert(explode,v);

		DM_copy_vert_data(dm,explode,j,v,1);
		*dest = source;

		if(i!=totpart) {
			/* get particle */
			pa= pars+i;

			/* get particle state */
			psys_particle_on_emitter(psmd,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,loc0,nor,0,0,0,0);
			mul_m4_v3(ob->obmat,loc0);

			state.time=cfra;
			psys_get_particle_state(&sim, i, &state, 1);

			vertco=CDDM_get_vert(explode,v)->co;
			
			mul_m4_v3(ob->obmat,vertco);

			VECSUB(vertco,vertco,loc0);

			/* apply rotation, size & location */
			mul_qt_v3(state.rot,vertco);
			mul_v3_fl(vertco,pa->size);
			VECADD(vertco,vertco,state.co);

			mul_m4_v3(imat,vertco);
		}
	}
	BLI_edgehashIterator_free(ehi);

	/*map new vertices to faces*/
	for (i=0; i<totface; i++) {
		MFace source;
		int orig_v4;

		if(facepa[i]!=totpart)
		{
			pa=pars+facepa[i];

			if(pa->alive==PARS_UNBORN && (emd->flag&eExplodeFlag_Unborn)==0) continue;
			if(pa->alive==PARS_ALIVE && (emd->flag&eExplodeFlag_Alive)==0) continue;
			if(pa->alive==PARS_DEAD && (emd->flag&eExplodeFlag_Dead)==0) continue;
		}

		dm->getFace(dm,i,&source);
		mf=CDDM_get_face(explode,i);
		
		orig_v4 = source.v4;

		if(facepa[i]!=totpart && cfra <= pa->time)
			mindex = totvert+totpart;
		else 
			mindex = totvert+facepa[i];

		source.v1 = edgesplit_get(vertpahash, source.v1, mindex);
		source.v2 = edgesplit_get(vertpahash, source.v2, mindex);
		source.v3 = edgesplit_get(vertpahash, source.v3, mindex);
		if(source.v4)
			source.v4 = edgesplit_get(vertpahash, source.v4, mindex);

		DM_copy_face_data(dm,explode,i,i,1);

		*mf = source;

		test_index_face(mf, &explode->faceData, i, (orig_v4 ? 4 : 3));
	}

	/* cleanup */
	BLI_edgehash_free(vertpahash, NULL);

	/* finalization */
	CDDM_calc_edges(explode);
	CDDM_calc_normals(explode);

	if(psmd->psys->lattice){
		end_latt_deform(psmd->psys->lattice);
		psmd->psys->lattice= NULL;
	}

	return explode;
}

static ParticleSystemModifierData * explodeModifier_findPrecedingParticlesystem(Object *ob, ModifierData *emd)
{
	ModifierData *md;
	ParticleSystemModifierData *psmd=0;

	for (md=ob->modifiers.first; emd!=md; md=md->next){
		if(md->type==eModifierType_ParticleSystem)
			psmd= (ParticleSystemModifierData*) md;
	}
	return psmd;
}
static DerivedMesh * explodeModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	ExplodeModifierData *emd= (ExplodeModifierData*) md;
	ParticleSystemModifierData *psmd=explodeModifier_findPrecedingParticlesystem(ob,md);

	if(psmd){
		ParticleSystem * psys=psmd->psys;

		if(psys==0 || psys->totpart==0) return derivedData;
		if(psys->part==0 || psys->particles==0) return derivedData;
		if(psmd->dm==0) return derivedData;

		/* 1. find faces to be exploded if needed */
		if(emd->facepa==0
				 || psmd->flag&eParticleSystemFlag_Pars
				 || emd->flag&eExplodeFlag_CalcFaces
				 || MEM_allocN_len(emd->facepa)/sizeof(int) != dm->getNumFaces(dm)){
			if(psmd->flag & eParticleSystemFlag_Pars)
				psmd->flag &= ~eParticleSystemFlag_Pars;
			
			if(emd->flag & eExplodeFlag_CalcFaces)
				emd->flag &= ~eExplodeFlag_CalcFaces;

			explodeModifier_createFacepa(emd,psmd,ob,derivedData);
				 }

				 /* 2. create new mesh */
				 if(emd->flag & eExplodeFlag_EdgeSplit){
					 int *facepa = emd->facepa;
					 DerivedMesh *splitdm=explodeModifier_splitEdges(emd,dm);
					 DerivedMesh *explode=explodeModifier_explodeMesh(emd, psmd, md->scene, ob, splitdm);

					 MEM_freeN(emd->facepa);
					 emd->facepa=facepa;
					 splitdm->release(splitdm);
					 return explode;
				 }
				 else
					 return explodeModifier_explodeMesh(emd, psmd, md->scene, ob, derivedData);
	}
	return derivedData;
}

/* Fluidsim */
static void fluidsimModifier_initData(ModifierData *md)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	
	fluidsim_init(fluidmd);
}
static void fluidsimModifier_freeData(ModifierData *md)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	
	fluidsim_free(fluidmd);
}

static void fluidsimModifier_copyData(ModifierData *md, ModifierData *target)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	FluidsimModifierData *tfluidmd= (FluidsimModifierData*) target;
	
	if(tfluidmd->fss)
		MEM_freeN(tfluidmd->fss);
	
	tfluidmd->fss = MEM_dupallocN(fluidmd->fss);
}

static DerivedMesh * fluidsimModifier_applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	DerivedMesh *result = NULL;
	
	/* check for alloc failing */
	if(!fluidmd->fss)
	{
		fluidsimModifier_initData(md);
		
		if(!fluidmd->fss)
			return derivedData;
	}

	result = fluidsimModifier_do(fluidmd, md->scene, ob, derivedData, useRenderParams, isFinalCalc);

	if(result) 
	{ 
		return result; 
	}
	
	return derivedData;
}

static void fluidsimModifier_updateDepgraph(
		ModifierData *md, DagForest *forest, Scene *scene,
      Object *ob, DagNode *obNode)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	Base *base;

	if(fluidmd && fluidmd->fss)
	{
		if(fluidmd->fss->type == OB_FLUIDSIM_DOMAIN)
		{
			for(base = scene->base.first; base; base= base->next) 
			{
				Object *ob1= base->object;
				if(ob1 != ob)
				{
					FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(ob1, eModifierType_Fluidsim);
					
					// only put dependancies from NON-DOMAIN fluids in here
					if(fluidmdtmp && fluidmdtmp->fss && (fluidmdtmp->fss->type!=OB_FLUIDSIM_DOMAIN))
					{
						DagNode *curNode = dag_get_node(forest, ob1);
						dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Fluidsim Object");
					}
				}
			}
		}
	}
}

static int fluidsimModifier_dependsOnTime(ModifierData *md) 
{
	return 1;
}

/* MeshDeform */

static void meshdeformModifier_initData(ModifierData *md)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	mmd->gridsize= 5;
}

static void meshdeformModifier_freeData(ModifierData *md)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	if(mmd->bindweights) MEM_freeN(mmd->bindweights);
	if(mmd->bindcos) MEM_freeN(mmd->bindcos);
	if(mmd->dyngrid) MEM_freeN(mmd->dyngrid);
	if(mmd->dyninfluences) MEM_freeN(mmd->dyninfluences);
	if(mmd->dynverts) MEM_freeN(mmd->dynverts);
}

static void meshdeformModifier_copyData(ModifierData *md, ModifierData *target)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;
	MeshDeformModifierData *tmmd = (MeshDeformModifierData*) target;

	tmmd->gridsize = mmd->gridsize;
	tmmd->object = mmd->object;
}

static CustomDataMask meshdeformModifier_requiredDataMask(Object *ob, ModifierData *md)
{	
	MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(mmd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static int meshdeformModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	return !mmd->object;
}

static void meshdeformModifier_foreachObjectLink(
		ModifierData *md, Object *ob,
  void (*walk)(void *userData, Object *ob, Object **obpoin),
	 void *userData)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	walk(userData, ob, &mmd->object);
}

static void meshdeformModifier_updateDepgraph(
					      ModifierData *md, DagForest *forest, Scene *scene, Object *ob,
	   DagNode *obNode)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	if (mmd->object) {
		DagNode *curNode = dag_get_node(forest, mmd->object);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA|DAG_RL_OB_DATA|DAG_RL_DATA_OB|DAG_RL_OB_OB,
				 "Mesh Deform Modifier");
	}
}

static float meshdeform_dynamic_bind(MeshDeformModifierData *mmd, float (*dco)[3], float *vec)
{
	MDefCell *cell;
	MDefInfluence *inf;
	float gridvec[3], dvec[3], ivec[3], co[3], wx, wy, wz;
	float weight, cageweight, totweight, *cageco;
	int i, j, a, x, y, z, size;

	co[0]= co[1]= co[2]= 0.0f;
	totweight= 0.0f;
	size= mmd->dyngridsize;

	for(i=0; i<3; i++) {
		gridvec[i]= (vec[i] - mmd->dyncellmin[i] - mmd->dyncellwidth*0.5f)/mmd->dyncellwidth;
		ivec[i]= (int)gridvec[i];
		dvec[i]= gridvec[i] - ivec[i];
	}

	for(i=0; i<8; i++) {
		if(i & 1) { x= ivec[0]+1; wx= dvec[0]; }
		else { x= ivec[0]; wx= 1.0f-dvec[0]; } 

		if(i & 2) { y= ivec[1]+1; wy= dvec[1]; }
		else { y= ivec[1]; wy= 1.0f-dvec[1]; } 

		if(i & 4) { z= ivec[2]+1; wz= dvec[2]; }
		else { z= ivec[2]; wz= 1.0f-dvec[2]; } 

		CLAMP(x, 0, size-1);
		CLAMP(y, 0, size-1);
		CLAMP(z, 0, size-1);

		a= x + y*size + z*size*size;
		weight= wx*wy*wz;

		cell= &mmd->dyngrid[a];
		inf= mmd->dyninfluences + cell->offset;
		for(j=0; j<cell->totinfluence; j++, inf++) {
			cageco= dco[inf->vertex];
			cageweight= weight*inf->weight;
			co[0] += cageweight*cageco[0];
			co[1] += cageweight*cageco[1];
			co[2] += cageweight*cageco[2];
			totweight += cageweight;
		}
	}

	VECCOPY(vec, co);

	return totweight;
}

static void meshdeformModifier_do(
				  ModifierData *md, Object *ob, DerivedMesh *dm,
      float (*vertexCos)[3], int numVerts)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;
	Mesh *me= (mmd->object)? mmd->object->data: NULL;
	EditMesh *em = (me)? BKE_mesh_get_editmesh(me): NULL;
	DerivedMesh *tmpdm, *cagedm;
	MDeformVert *dvert = NULL;
	MDeformWeight *dw;
	MVert *cagemvert;
	float imat[4][4], cagemat[4][4], iobmat[4][4], icagemat[3][3], cmat[4][4];
	float weight, totweight, fac, co[3], *weights, (*dco)[3], (*bindcos)[3];
	int a, b, totvert, totcagevert, defgrp_index;
	
	if(!mmd->object || (!mmd->bindcos && !mmd->bindfunc))
		return;
	
	/* get cage derivedmesh */
	if(em) {
		tmpdm= editmesh_get_derived_cage_and_final(md->scene, ob, em, &cagedm, 0);
		if(tmpdm)
			tmpdm->release(tmpdm);
		BKE_mesh_end_editmesh(me, em);
	}
	else
		cagedm= mmd->object->derivedFinal;

	/* if we don't have one computed, use derivedmesh from data
	 * without any modifiers */
	if(!cagedm) {
		cagedm= get_dm(md->scene, mmd->object, NULL, NULL, NULL, 0);
		if(cagedm)
			cagedm->needsFree= 1;
	}
	
	if(!cagedm)
		return;

	/* compute matrices to go in and out of cage object space */
	invert_m4_m4(imat, mmd->object->obmat);
	mul_m4_m4m4(cagemat, ob->obmat, imat);
	mul_m4_m4m4(cmat, cagemat, mmd->bindmat);
	invert_m4_m4(iobmat, cmat);
	copy_m3_m4(icagemat, iobmat);

	/* bind weights if needed */
	if(!mmd->bindcos) {
		static int recursive = 0;

		/* progress bar redraw can make this recursive .. */
		if(!recursive) {
			recursive = 1;
			mmd->bindfunc(md->scene, dm, mmd, (float*)vertexCos, numVerts, cagemat);
			recursive = 0;
		}
	}

	/* verify we have compatible weights */
	totvert= numVerts;
	totcagevert= cagedm->getNumVerts(cagedm);

	if(mmd->totvert!=totvert || mmd->totcagevert!=totcagevert || !mmd->bindcos) {
		cagedm->release(cagedm);
		return;
	}
	
	/* setup deformation data */
	cagemvert= cagedm->getVertArray(cagedm);
	weights= mmd->bindweights;
	bindcos= (float(*)[3])mmd->bindcos;

	dco= MEM_callocN(sizeof(*dco)*totcagevert, "MDefDco");
	for(a=0; a<totcagevert; a++) {
		/* get cage vertex in world space with binding transform */
		VECCOPY(co, cagemvert[a].co);

		if(G.rt != 527) {
			mul_m4_v3(mmd->bindmat, co);
			/* compute difference with world space bind coord */
			VECSUB(dco[a], co, bindcos[a]);
		}
		else
			VECCOPY(dco[a], co)
	}

	defgrp_index = -1;

	if(mmd->defgrp_name[0]) {
		bDeformGroup *def;

		for(a=0, def=ob->defbase.first; def; def=def->next, a++) {
			if(!strcmp(def->name, mmd->defgrp_name)) {
				defgrp_index= a;
				break;
			}
		}

		if (defgrp_index >= 0)
			dvert= dm->getVertDataArray(dm, CD_MDEFORMVERT);
	}

	/* do deformation */
	fac= 1.0f;

	for(b=0; b<totvert; b++) {
		if(mmd->flag & MOD_MDEF_DYNAMIC_BIND)
			if(!mmd->dynverts[b])
				continue;

		if(dvert) {
			for(dw=NULL, a=0; a<dvert[b].totweight; a++) {
				if(dvert[b].dw[a].def_nr == defgrp_index) {
					dw = &dvert[b].dw[a];
					break;
				}
			}

			if(mmd->flag & MOD_MDEF_INVERT_VGROUP) {
				if(!dw) fac= 1.0f;
				else if(dw->weight == 1.0f) continue;
				else fac=1.0f-dw->weight;
			}
			else {
				if(!dw) continue;
				else fac= dw->weight;
			}
		}

		if(mmd->flag & MOD_MDEF_DYNAMIC_BIND) {
			/* transform coordinate into cage's local space */
			VECCOPY(co, vertexCos[b]);
			mul_m4_v3(cagemat, co);
			totweight= meshdeform_dynamic_bind(mmd, dco, co);
		}
		else {
			totweight= 0.0f;
			co[0]= co[1]= co[2]= 0.0f;

			for(a=0; a<totcagevert; a++) {
				weight= weights[a + b*totcagevert];
				co[0]+= weight*dco[a][0];
				co[1]+= weight*dco[a][1];
				co[2]+= weight*dco[a][2];
				totweight += weight;
			}
		}

		if(totweight > 0.0f) {
			mul_v3_fl(co, fac/totweight);
			mul_m3_v3(icagemat, co);
			if(G.rt != 527)
				VECADD(vertexCos[b], vertexCos[b], co)
						else
						VECCOPY(vertexCos[b], co)
		}
	}

	/* release cage derivedmesh */
	MEM_freeN(dco);
	cagedm->release(cagedm);
}

static void meshdeformModifier_deformVerts(
					   ModifierData *md, Object *ob, DerivedMesh *derivedData,
	float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm= get_dm(md->scene, ob, NULL, derivedData, NULL, 0);;

	if(!dm)
		return;

	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */
	
	meshdeformModifier_do(md, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void meshdeformModifier_deformVertsEM(
					     ModifierData *md, Object *ob, EditMesh *editData,
	  DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm;

	if(!derivedData && ob->type == OB_MESH)
		dm = CDDM_from_editmesh(editData, ob->data);
	else
		dm = derivedData;

	meshdeformModifier_do(md, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

/* Multires */
static void multiresModifier_initData(ModifierData *md)
{
	MultiresModifierData *mmd = (MultiresModifierData*)md;

	mmd->lvl = 0;
	mmd->sculptlvl = 0;
	mmd->renderlvl = 0;
	mmd->totlvl = 0;
}

static void multiresModifier_copyData(ModifierData *md, ModifierData *target)
{
	MultiresModifierData *mmd = (MultiresModifierData*) md;
	MultiresModifierData *tmmd = (MultiresModifierData*) target;

	tmmd->lvl = mmd->lvl;
	tmmd->sculptlvl = mmd->sculptlvl;
	tmmd->renderlvl = mmd->renderlvl;
	tmmd->totlvl = mmd->totlvl;
}

static DerivedMesh *multiresModifier_applyModifier(ModifierData *md, Object *ob, DerivedMesh *dm,
						   int useRenderParams, int isFinalCalc)
{
	MultiresModifierData *mmd = (MultiresModifierData*)md;
	DerivedMesh *result;

	result = multires_dm_create_from_derived(mmd, 0, dm, ob, useRenderParams, isFinalCalc);

	if(result == dm)
		return dm;

	if(useRenderParams || !isFinalCalc) {
		DerivedMesh *cddm= CDDM_copy(result);
		result->release(result);
		result= cddm;
	}
	else if(ob->mode & OB_MODE_SCULPT) {
		/* would be created on the fly too, just nicer this
		   way on first stroke after e.g. switching levels */
		result->getPBVH(ob, result);
	}

	return result;
}

/* Shrinkwrap */

static void shrinkwrapModifier_initData(ModifierData *md)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;
	smd->shrinkType = MOD_SHRINKWRAP_NEAREST_SURFACE;
	smd->shrinkOpts = MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR;
	smd->keepDist	= 0.0f;

	smd->target		= NULL;
	smd->auxTarget	= NULL;
}

static void shrinkwrapModifier_copyData(ModifierData *md, ModifierData *target)
{
	ShrinkwrapModifierData *smd  = (ShrinkwrapModifierData*)md;
	ShrinkwrapModifierData *tsmd = (ShrinkwrapModifierData*)target;

	tsmd->target	= smd->target;
	tsmd->auxTarget = smd->auxTarget;

	strcpy(tsmd->vgroup_name, smd->vgroup_name);

	tsmd->keepDist	= smd->keepDist;
	tsmd->shrinkType= smd->shrinkType;
	tsmd->shrinkOpts= smd->shrinkOpts;
	tsmd->projAxis = smd->projAxis;
	tsmd->subsurfLevels = smd->subsurfLevels;
}

static CustomDataMask shrinkwrapModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(smd->vgroup_name[0])
		dataMask |= (1 << CD_MDEFORMVERT);

	if(smd->shrinkType == MOD_SHRINKWRAP_PROJECT
	&& smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL)
		dataMask |= (1 << CD_MVERT);
		
	return dataMask;
}

static int shrinkwrapModifier_isDisabled(ModifierData *md, int useRenderParams)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;
	return !smd->target;
}


static void shrinkwrapModifier_foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;

	walk(userData, ob, &smd->target);
	walk(userData, ob, &smd->auxTarget);
}

static void shrinkwrapModifier_deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = shrinkwrapModifier_requiredDataMask(ob, md);

	/* ensure we get a CDDM with applied vertex coords */
	if(dataMask)
		dm= get_cddm(md->scene, ob, NULL, dm, vertexCos);

	shrinkwrapModifier_deform((ShrinkwrapModifierData*)md, md->scene, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void shrinkwrapModifier_deformVertsEM(ModifierData *md, Object *ob, EditMesh *editData, DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = shrinkwrapModifier_requiredDataMask(ob, md);

	/* ensure we get a CDDM with applied vertex coords */
	if(dataMask)
		dm= get_cddm(md->scene, ob, editData, dm, vertexCos);

	shrinkwrapModifier_deform((ShrinkwrapModifierData*)md, md->scene, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void shrinkwrapModifier_updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene, Object *ob, DagNode *obNode)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;

	if (smd->target)
		dag_add_relation(forest, dag_get_node(forest, smd->target),   obNode, DAG_RL_OB_DATA | DAG_RL_DATA_DATA, "Shrinkwrap Modifier");

	if (smd->auxTarget)
		dag_add_relation(forest, dag_get_node(forest, smd->auxTarget), obNode, DAG_RL_OB_DATA | DAG_RL_DATA_DATA, "Shrinkwrap Modifier");
}

/* SimpleDeform */
static void simpledeformModifier_initData(ModifierData *md)
{
	SimpleDeformModifierData *smd = (SimpleDeformModifierData*) md;

	smd->mode = MOD_SIMPLEDEFORM_MODE_TWIST;
	smd->axis = 0;

	smd->origin   =  NULL;
	smd->factor   =  0.35f;
	smd->limit[0] =  0.0f;
	smd->limit[1] =  1.0f;
}

static void simpledeformModifier_copyData(ModifierData *md, ModifierData *target)
{
	SimpleDeformModifierData *smd  = (SimpleDeformModifierData*)md;
	SimpleDeformModifierData *tsmd = (SimpleDeformModifierData*)target;

	tsmd->mode	= smd->mode;
	tsmd->axis  = smd->axis;
	tsmd->origin= smd->origin;
	tsmd->factor= smd->factor;
	memcpy(tsmd->limit, smd->limit, sizeof(tsmd->limit));
}

static CustomDataMask simpledeformModifier_requiredDataMask(Object *ob, ModifierData *md)
{
	SimpleDeformModifierData *smd = (SimpleDeformModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(smd->vgroup_name[0])
		dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void simpledeformModifier_foreachObjectLink(ModifierData *md, Object *ob, void (*walk)(void *userData, Object *ob, Object **obpoin), void *userData)
{
	SimpleDeformModifierData *smd  = (SimpleDeformModifierData*)md;
	walk(userData, ob, &smd->origin);
}

static void simpledeformModifier_updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene, Object *ob, DagNode *obNode)
{
	SimpleDeformModifierData *smd  = (SimpleDeformModifierData*)md;

	if (smd->origin)
		dag_add_relation(forest, dag_get_node(forest, smd->origin), obNode, DAG_RL_OB_DATA, "SimpleDeform Modifier");
}

static void simpledeformModifier_deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = simpledeformModifier_requiredDataMask(ob, md);

	/* we implement requiredDataMask but thats not really usefull since
	   mesh_calc_modifiers pass a NULL derivedData */
	if(dataMask)
		dm= get_dm(md->scene, ob, NULL, dm, NULL, 0);

	SimpleDeformModifier_do((SimpleDeformModifierData*)md, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void simpledeformModifier_deformVertsEM(ModifierData *md, Object *ob, EditMesh *editData, DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = simpledeformModifier_requiredDataMask(ob, md);

	/* we implement requiredDataMask but thats not really usefull since
	   mesh_calc_modifiers pass a NULL derivedData */
	if(dataMask)
		dm= get_dm(md->scene, ob, editData, dm, NULL, 0);

	SimpleDeformModifier_do((SimpleDeformModifierData*)md, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

/* Shape Key */

static void shapekeyModifier_deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
      float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	KeyBlock *kb= ob_get_keyblock(ob);
	float (*deformedVerts)[3];

	if(kb && kb->totelem == numVerts) {
		deformedVerts= (float(*)[3])do_ob_key(md->scene, ob);
		if(deformedVerts) {
			memcpy(vertexCos, deformedVerts, sizeof(float)*3*numVerts);
			MEM_freeN(deformedVerts);
		}
	}
}

static void shapekeyModifier_deformVertsEM(
					   ModifierData *md, Object *ob, EditMesh *editData,
	DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	Key *key= ob_get_key(ob);

	if(key && key->type == KEY_RELATIVE)
		shapekeyModifier_deformVerts(md, ob, derivedData, vertexCos, numVerts, 0, 0);
}

static void shapekeyModifier_deformMatricesEM(
					      ModifierData *md, Object *ob, EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3],
					     float (*defMats)[3][3], int numVerts)
{
	Key *key= ob_get_key(ob);
	KeyBlock *kb= ob_get_keyblock(ob);
	float scale[3][3];
	int a;

	if(kb && kb->totelem==numVerts && kb!=key->refkey) {
		scale_m3_fl(scale, kb->curval);

		for(a=0; a<numVerts; a++)
			copy_m3_m3(defMats[a], scale);
	}
}

/***/

static ModifierTypeInfo typeArr[NUM_MODIFIER_TYPES];
static int typeArrInit = 1;

ModifierTypeInfo *modifierType_getInfo(ModifierType type)
{
	if (typeArrInit) {
		ModifierTypeInfo *mti;

		memset(typeArr, 0, sizeof(typeArr));

		/* Initialize and return the appropriate type info structure,
		 * assumes that modifier has:
		*  name == typeName, 
		*  structName == typeName + 'ModifierData'
		*/
#define INIT_TYPE(typeName) \
		(strcpy(typeArr[eModifierType_##typeName].name, #typeName), \
		strcpy(typeArr[eModifierType_##typeName].structName, \
#typeName "ModifierData"), \
		typeArr[eModifierType_##typeName].structSize = \
		sizeof(typeName##ModifierData), \
		&typeArr[eModifierType_##typeName])

		mti = &typeArr[eModifierType_None];
		strcpy(mti->name, "None");
		strcpy(mti->structName, "ModifierData");
		mti->structSize = sizeof(ModifierData);
		mti->type = eModifierType_None;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_AcceptsCVs;
		mti->isDisabled = noneModifier_isDisabled;
		
		mti = INIT_TYPE(Curve);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode;
		mti->initData = curveModifier_initData;
		mti->copyData = curveModifier_copyData;
		mti->requiredDataMask = curveModifier_requiredDataMask;
		mti->isDisabled = curveModifier_isDisabled;
		mti->foreachObjectLink = curveModifier_foreachObjectLink;
		mti->updateDepgraph = curveModifier_updateDepgraph;
		mti->deformVerts = curveModifier_deformVerts;
		mti->deformVertsEM = curveModifier_deformVertsEM;

		mti = INIT_TYPE(Lattice);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode;
		mti->copyData = latticeModifier_copyData;
		mti->requiredDataMask = latticeModifier_requiredDataMask;
		mti->isDisabled = latticeModifier_isDisabled;
		mti->foreachObjectLink = latticeModifier_foreachObjectLink;
		mti->updateDepgraph = latticeModifier_updateDepgraph;
		mti->deformVerts = latticeModifier_deformVerts;
		mti->deformVertsEM = latticeModifier_deformVertsEM;

		mti = INIT_TYPE(Subsurf);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsMapping
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = subsurfModifier_initData;
		mti->copyData = subsurfModifier_copyData;
		mti->freeData = subsurfModifier_freeData;
		mti->isDisabled = subsurfModifier_isDisabled;
		mti->applyModifier = subsurfModifier_applyModifier;
		mti->applyModifierEM = subsurfModifier_applyModifierEM;

		mti = INIT_TYPE(Build);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->initData = buildModifier_initData;
		mti->copyData = buildModifier_copyData;
		mti->dependsOnTime = buildModifier_dependsOnTime;
		mti->applyModifier = buildModifier_applyModifier;
		
		mti = INIT_TYPE(Mask);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->copyData = maskModifier_copyData;
		mti->requiredDataMask= maskModifier_requiredDataMask;
		mti->foreachObjectLink = maskModifier_foreachObjectLink;
		mti->updateDepgraph = maskModifier_updateDepgraph;
		mti->applyModifier = maskModifier_applyModifier;

		mti = INIT_TYPE(Array);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsMapping
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = arrayModifier_initData;
		mti->copyData = arrayModifier_copyData;
		mti->foreachObjectLink = arrayModifier_foreachObjectLink;
		mti->updateDepgraph = arrayModifier_updateDepgraph;
		mti->applyModifier = arrayModifier_applyModifier;
		mti->applyModifierEM = arrayModifier_applyModifierEM;

		mti = INIT_TYPE(Mirror);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsMapping
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = mirrorModifier_initData;
		mti->copyData = mirrorModifier_copyData;
		mti->foreachObjectLink = mirrorModifier_foreachObjectLink;
		mti->updateDepgraph = mirrorModifier_updateDepgraph;
		mti->applyModifier = mirrorModifier_applyModifier;
		mti->applyModifierEM = mirrorModifier_applyModifierEM;

		mti = INIT_TYPE(EdgeSplit);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsMapping
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = edgesplitModifier_initData;
		mti->copyData = edgesplitModifier_copyData;
		mti->applyModifier = edgesplitModifier_applyModifier;
		mti->applyModifierEM = edgesplitModifier_applyModifierEM;

		mti = INIT_TYPE(Bevel);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = bevelModifier_initData;
		mti->copyData = bevelModifier_copyData;
		mti->requiredDataMask = bevelModifier_requiredDataMask;
		mti->applyModifier = bevelModifier_applyModifier;
		mti->applyModifierEM = bevelModifier_applyModifierEM;

		mti = INIT_TYPE(Displace);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsMesh|eModifierTypeFlag_SupportsEditmode;
		mti->initData = displaceModifier_initData;
		mti->copyData = displaceModifier_copyData;
		mti->requiredDataMask = displaceModifier_requiredDataMask;
		mti->dependsOnTime = displaceModifier_dependsOnTime;
		mti->foreachObjectLink = displaceModifier_foreachObjectLink;
		mti->foreachIDLink = displaceModifier_foreachIDLink;
		mti->updateDepgraph = displaceModifier_updateDepgraph;
		mti->isDisabled = displaceModifier_isDisabled;
		mti->deformVerts = displaceModifier_deformVerts;
		mti->deformVertsEM = displaceModifier_deformVertsEM;

		mti = INIT_TYPE(UVProject);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsMapping
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = uvprojectModifier_initData;
		mti->copyData = uvprojectModifier_copyData;
		mti->requiredDataMask = uvprojectModifier_requiredDataMask;
		mti->foreachObjectLink = uvprojectModifier_foreachObjectLink;
		mti->foreachIDLink = uvprojectModifier_foreachIDLink;
		mti->updateDepgraph = uvprojectModifier_updateDepgraph;
		mti->applyModifier = uvprojectModifier_applyModifier;
		mti->applyModifierEM = uvprojectModifier_applyModifierEM;

		mti = INIT_TYPE(Decimate);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->initData = decimateModifier_initData;
		mti->copyData = decimateModifier_copyData;
		mti->applyModifier = decimateModifier_applyModifier;

		mti = INIT_TYPE(Smooth);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsEditmode;
		mti->initData = smoothModifier_initData;
		mti->copyData = smoothModifier_copyData;
		mti->requiredDataMask = smoothModifier_requiredDataMask;
		mti->isDisabled = smoothModifier_isDisabled;
		mti->deformVerts = smoothModifier_deformVerts;
		mti->deformVertsEM = smoothModifier_deformVertsEM;

		mti = INIT_TYPE(Cast);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode;
		mti->initData = castModifier_initData;
		mti->copyData = castModifier_copyData;
		mti->requiredDataMask = castModifier_requiredDataMask;
		mti->isDisabled = castModifier_isDisabled;
		mti->foreachObjectLink = castModifier_foreachObjectLink;
		mti->updateDepgraph = castModifier_updateDepgraph;
		mti->deformVerts = castModifier_deformVerts;
		mti->deformVertsEM = castModifier_deformVertsEM;

		mti = INIT_TYPE(Wave);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode;
		mti->initData = waveModifier_initData;
		mti->copyData = waveModifier_copyData;
		mti->dependsOnTime = waveModifier_dependsOnTime;
		mti->requiredDataMask = waveModifier_requiredDataMask;
		mti->foreachObjectLink = waveModifier_foreachObjectLink;
		mti->foreachIDLink = waveModifier_foreachIDLink;
		mti->updateDepgraph = waveModifier_updateDepgraph;
		mti->deformVerts = waveModifier_deformVerts;
		mti->deformVertsEM = waveModifier_deformVertsEM;

		mti = INIT_TYPE(Armature);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode;
		mti->initData = armatureModifier_initData;
		mti->copyData = armatureModifier_copyData;
		mti->requiredDataMask = armatureModifier_requiredDataMask;
		mti->isDisabled = armatureModifier_isDisabled;
		mti->foreachObjectLink = armatureModifier_foreachObjectLink;
		mti->updateDepgraph = armatureModifier_updateDepgraph;
		mti->deformVerts = armatureModifier_deformVerts;
		mti->deformVertsEM = armatureModifier_deformVertsEM;
		mti->deformMatricesEM = armatureModifier_deformMatricesEM;

		mti = INIT_TYPE(Hook);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode;
		mti->initData = hookModifier_initData;
		mti->copyData = hookModifier_copyData;
		mti->requiredDataMask = hookModifier_requiredDataMask;
		mti->freeData = hookModifier_freeData;
		mti->isDisabled = hookModifier_isDisabled;
		mti->foreachObjectLink = hookModifier_foreachObjectLink;
		mti->updateDepgraph = hookModifier_updateDepgraph;
		mti->deformVerts = hookModifier_deformVerts;
		mti->deformVertsEM = hookModifier_deformVertsEM;

		mti = INIT_TYPE(Softbody);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_RequiresOriginalData
				| eModifierTypeFlag_Single;
		mti->deformVerts = softbodyModifier_deformVerts;
		mti->dependsOnTime = softbodyModifier_dependsOnTime;
		
		mti = INIT_TYPE(Smoke);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->initData = smokeModifier_initData;
		mti->freeData = smokeModifier_freeData; 
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_UsesPointCache
				| eModifierTypeFlag_Single;
		mti->deformVerts = smokeModifier_deformVerts;
		mti->dependsOnTime = smokeModifier_dependsOnTime;
		mti->updateDepgraph = smokeModifier_updateDepgraph;
	
		mti = INIT_TYPE(Cloth);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->initData = clothModifier_initData;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_UsesPointCache
				| eModifierTypeFlag_Single;
		mti->dependsOnTime = clothModifier_dependsOnTime;
		mti->freeData = clothModifier_freeData; 
		mti->requiredDataMask = clothModifier_requiredDataMask;
		mti->copyData = clothModifier_copyData;
		mti->applyModifier = clothModifier_applyModifier;
		mti->updateDepgraph = clothModifier_updateDepgraph;
		
		mti = INIT_TYPE(Collision);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->initData = collisionModifier_initData;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_Single;
		mti->dependsOnTime = collisionModifier_dependsOnTime;
		mti->freeData = collisionModifier_freeData; 
		mti->deformVerts = collisionModifier_deformVerts;
		// mti->copyData = collisionModifier_copyData;

		mti = INIT_TYPE(Surface);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->initData = surfaceModifier_initData;
		mti->flags = eModifierTypeFlag_AcceptsMesh|eModifierTypeFlag_NoUserAdd;
		mti->dependsOnTime = surfaceModifier_dependsOnTime;
		mti->freeData = surfaceModifier_freeData; 
		mti->deformVerts = surfaceModifier_deformVerts;

		mti = INIT_TYPE(Boolean);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_UsesPointCache;
		mti->copyData = booleanModifier_copyData;
		mti->isDisabled = booleanModifier_isDisabled;
		mti->applyModifier = booleanModifier_applyModifier;
		mti->foreachObjectLink = booleanModifier_foreachObjectLink;
		mti->updateDepgraph = booleanModifier_updateDepgraph;
		mti->requiredDataMask = booleanModifier_requiredDataMask;

		mti = INIT_TYPE(MeshDeform);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode;
		mti->initData = meshdeformModifier_initData;
		mti->freeData = meshdeformModifier_freeData;
		mti->copyData = meshdeformModifier_copyData;
		mti->requiredDataMask = meshdeformModifier_requiredDataMask;
		mti->isDisabled = meshdeformModifier_isDisabled;
		mti->foreachObjectLink = meshdeformModifier_foreachObjectLink;
		mti->updateDepgraph = meshdeformModifier_updateDepgraph;
		mti->deformVerts = meshdeformModifier_deformVerts;
		mti->deformVertsEM = meshdeformModifier_deformVertsEM;

		mti = INIT_TYPE(ParticleSystem);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsMapping
				| eModifierTypeFlag_UsesPointCache;
#if 0
		| eModifierTypeFlag_SupportsEditmode;
		|eModifierTypeFlag_EnableInEditmode;
#endif
		mti->initData = particleSystemModifier_initData;
		mti->freeData = particleSystemModifier_freeData;
		mti->copyData = particleSystemModifier_copyData;
		mti->deformVerts = particleSystemModifier_deformVerts;
#if 0
		mti->deformVertsEM = particleSystemModifier_deformVertsEM;
#endif
		mti->requiredDataMask = particleSystemModifier_requiredDataMask;

		mti = INIT_TYPE(ParticleInstance);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsMapping
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = particleInstanceModifier_initData;
		mti->copyData = particleInstanceModifier_copyData;
		mti->dependsOnTime = particleInstanceModifier_dependsOnTime;
		mti->foreachObjectLink = particleInstanceModifier_foreachObjectLink;
		mti->applyModifier = particleInstanceModifier_applyModifier;
		mti->applyModifierEM = particleInstanceModifier_applyModifierEM;
		mti->updateDepgraph = particleInstanceModifier_updateDepgraph;

		mti = INIT_TYPE(Explode);
		mti->type = eModifierTypeType_Nonconstructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->initData = explodeModifier_initData;
		mti->freeData = explodeModifier_freeData;
		mti->copyData = explodeModifier_copyData;
		mti->dependsOnTime = explodeModifier_dependsOnTime;
		mti->requiredDataMask = explodeModifier_requiredDataMask;
		mti->applyModifier = explodeModifier_applyModifier;
		
		mti = INIT_TYPE(Fluidsim);
		mti->type = eModifierTypeType_Nonconstructive
				| eModifierTypeFlag_RequiresOriginalData
				| eModifierTypeFlag_Single;
		mti->flags = eModifierTypeFlag_AcceptsMesh;
		mti->initData = fluidsimModifier_initData;
		mti->freeData = fluidsimModifier_freeData;
		mti->copyData = fluidsimModifier_copyData;
		mti->dependsOnTime = fluidsimModifier_dependsOnTime;
		mti->applyModifier = fluidsimModifier_applyModifier;
		mti->updateDepgraph = fluidsimModifier_updateDepgraph;

		mti = INIT_TYPE(Shrinkwrap);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = shrinkwrapModifier_initData;
		mti->copyData = shrinkwrapModifier_copyData;
		mti->requiredDataMask = shrinkwrapModifier_requiredDataMask;
		mti->isDisabled = shrinkwrapModifier_isDisabled;
		mti->foreachObjectLink = shrinkwrapModifier_foreachObjectLink;
		mti->deformVerts = shrinkwrapModifier_deformVerts;
		mti->deformVertsEM = shrinkwrapModifier_deformVertsEM;
		mti->updateDepgraph = shrinkwrapModifier_updateDepgraph;

		mti = INIT_TYPE(SimpleDeform);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_AcceptsCVs				
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = simpledeformModifier_initData;
		mti->copyData = simpledeformModifier_copyData;
		mti->requiredDataMask = simpledeformModifier_requiredDataMask;
		mti->deformVerts = simpledeformModifier_deformVerts;
		mti->deformVertsEM = simpledeformModifier_deformVertsEM;
		mti->foreachObjectLink = simpledeformModifier_foreachObjectLink;
		mti->updateDepgraph = simpledeformModifier_updateDepgraph;

		mti = INIT_TYPE(Multires);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_RequiresOriginalData;
		mti->initData = multiresModifier_initData;
		mti->copyData = multiresModifier_copyData;
		mti->applyModifier = multiresModifier_applyModifier;

		mti = INIT_TYPE(ShapeKey);
		mti->type = eModifierTypeType_OnlyDeform;
		mti->flags = eModifierTypeFlag_AcceptsCVs
				| eModifierTypeFlag_SupportsEditmode;
		mti->deformVerts = shapekeyModifier_deformVerts;
		mti->deformVertsEM = shapekeyModifier_deformVertsEM;
		mti->deformMatricesEM = shapekeyModifier_deformMatricesEM;

		mti = INIT_TYPE(Solidify);
		mti->type = eModifierTypeType_Constructive;
		mti->flags = eModifierTypeFlag_AcceptsMesh
				| eModifierTypeFlag_SupportsMapping
				| eModifierTypeFlag_SupportsEditmode
				| eModifierTypeFlag_EnableInEditmode;
		mti->initData = solidifyModifier_initData;
		mti->copyData = solidifyModifier_copyData;
		mti->applyModifier = solidifyModifier_applyModifier;
		mti->applyModifierEM = solidifyModifier_applyModifierEM;
		typeArrInit = 0;
#undef INIT_TYPE
	}

	if (type>=0 && type<NUM_MODIFIER_TYPES && typeArr[type].name[0]!='\0') {
		return &typeArr[type];
	} else {
		return NULL;
	}
}

/***/

ModifierData *modifier_new(int type)
{
	ModifierTypeInfo *mti = modifierType_getInfo(type);
	ModifierData *md = MEM_callocN(mti->structSize, mti->structName);
	
	// FIXME: we need to make the name always be unique somehow...
	strcpy(md->name, mti->name);

	md->type = type;
	md->mode = eModifierMode_Realtime
			| eModifierMode_Render | eModifierMode_Expanded;

	if (mti->flags & eModifierTypeFlag_EnableInEditmode)
		md->mode |= eModifierMode_Editmode;

	if (mti->initData) mti->initData(md);

	return md;
}

void modifier_free(ModifierData *md) 
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	if (mti->freeData) mti->freeData(md);
	if (md->error) MEM_freeN(md->error);

	MEM_freeN(md);
}

void modifier_unique_name(ListBase *modifiers, ModifierData *md)
{
	if (modifiers && md) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		
		BLI_uniquename(modifiers, md, mti->name, '.', offsetof(ModifierData, name), sizeof(md->name));
	}
}

int modifier_dependsOnTime(ModifierData *md) 
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	return mti->dependsOnTime && mti->dependsOnTime(md);
}

int modifier_supportsMapping(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	return (mti->type==eModifierTypeType_OnlyDeform ||
			(mti->flags & eModifierTypeFlag_SupportsMapping));
}

ModifierData *modifiers_findByType(Object *ob, ModifierType type)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md=md->next)
		if (md->type==type)
			break;

	return md;
}

void modifiers_clearErrors(Object *ob)
{
	ModifierData *md = ob->modifiers.first;
	int qRedraw = 0;

	for (; md; md=md->next) {
		if (md->error) {
			MEM_freeN(md->error);
			md->error = NULL;

			qRedraw = 1;
		}
	}
}

void modifiers_foreachObjectLink(Object *ob, ObjectWalkFunc walk,
				 void *userData)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (mti->foreachObjectLink)
			mti->foreachObjectLink(md, ob, walk, userData);
	}
}

void modifiers_foreachIDLink(Object *ob, IDWalkFunc walk, void *userData)
{
	ModifierData *md = ob->modifiers.first;

	for (; md; md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if(mti->foreachIDLink) mti->foreachIDLink(md, ob, walk, userData);
		else if(mti->foreachObjectLink) {
			/* each Object can masquerade as an ID, so this should be OK */
			ObjectWalkFunc fp = (ObjectWalkFunc)walk;
			mti->foreachObjectLink(md, ob, fp, userData);
		}
	}
}

void modifier_copyData(ModifierData *md, ModifierData *target)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	target->mode = md->mode;

	if (mti->copyData)
		mti->copyData(md, target);
}

int modifier_couldBeCage(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	return (	(md->mode & eModifierMode_Realtime) &&
			(md->mode & eModifierMode_Editmode) &&
			(!mti->isDisabled || !mti->isDisabled(md, 0)) &&
			modifier_supportsMapping(md));	
}

int modifier_sameTopology(ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	return ( mti->type == eModifierTypeType_OnlyDeform || mti->type == eModifierTypeType_Nonconstructive);
}

void modifier_setError(ModifierData *md, char *format, ...)
{
	char buffer[2048];
	va_list ap;

	va_start(ap, format);
	vsprintf(buffer, format, ap);
	va_end(ap);

	if (md->error)
		MEM_freeN(md->error);

	md->error = BLI_strdup(buffer);

}

/* used for buttons, to find out if the 'draw deformed in editmode' option is
 * there
 * 
 * also used in transform_conversion.c, to detect CrazySpace [tm] (2nd arg
 * then is NULL)
 */
int modifiers_getCageIndex(Object *ob, int *lastPossibleCageIndex_r, int virtual_)
{
	ModifierData *md = (virtual_)? modifiers_getVirtualModifierList(ob): ob->modifiers.first;
	int i, cageIndex = -1;

	/* Find the last modifier acting on the cage. */
	for (i=0; md; i++,md=md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if (!(md->mode & eModifierMode_Realtime)) continue;
		if (!(md->mode & eModifierMode_Editmode)) continue;
		if (mti->isDisabled && mti->isDisabled(md, 0)) continue;
		if (!(mti->flags & eModifierTypeFlag_SupportsEditmode)) continue;
		if (md->mode & eModifierMode_DisableTemporary) continue;

		if (!modifier_supportsMapping(md))
			break;

		if (lastPossibleCageIndex_r) *lastPossibleCageIndex_r = i;
		if (md->mode & eModifierMode_OnCage)
			cageIndex = i;
	}

	return cageIndex;
}


int modifiers_isSoftbodyEnabled(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Softbody);

	return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

int modifiers_isClothEnabled(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Cloth);

	return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

int modifiers_isParticleEnabled(Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_ParticleSystem);

	return (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render));
}

int modifier_isEnabled(ModifierData *md, int required_mode)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);

	if((md->mode & required_mode) != required_mode) return 0;
	if(mti->isDisabled && mti->isDisabled(md, required_mode == eModifierMode_Render)) return 0;
	if(md->mode & eModifierMode_DisableTemporary) return 0;
	if(required_mode & eModifierMode_Editmode)
		if(!(mti->flags & eModifierTypeFlag_SupportsEditmode)) return 0;
	
	return 1;
}

LinkNode *modifiers_calcDataMasks(Object *ob, ModifierData *md, CustomDataMask dataMask, int required_mode)
{
	LinkNode *dataMasks = NULL;
	LinkNode *curr, *prev;

	/* build a list of modifier data requirements in reverse order */
	for(; md; md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);
		CustomDataMask mask = 0;

		if(modifier_isEnabled(md, required_mode))
			if(mti->requiredDataMask)
				mask = mti->requiredDataMask(ob, md);

		BLI_linklist_prepend(&dataMasks, SET_INT_IN_POINTER(mask));
	}

	/* build the list of required data masks - each mask in the list must
	* include all elements of the masks that follow it
	*
	* note the list is currently in reverse order, so "masks that follow it"
	* actually means "masks that precede it" at the moment
	*/
	for(curr = dataMasks, prev = NULL; curr; prev = curr, curr = curr->next) {
		if(prev) {
			CustomDataMask prev_mask = (CustomDataMask)GET_INT_FROM_POINTER(prev->link);
			CustomDataMask curr_mask = (CustomDataMask)GET_INT_FROM_POINTER(curr->link);

			curr->link = SET_INT_IN_POINTER(curr_mask | prev_mask);
		} else {
			CustomDataMask curr_mask = (CustomDataMask)GET_INT_FROM_POINTER(curr->link);

			curr->link = SET_INT_IN_POINTER(curr_mask | dataMask);
		}
	}

	/* reverse the list so it's in the correct order */
	BLI_linklist_reverse(&dataMasks);

	return dataMasks;
}

ModifierData *modifiers_getVirtualModifierList(Object *ob)
{
		/* Kinda hacky, but should be fine since we are never
	* reentrant and avoid free hassles.
		*/
	static ArmatureModifierData amd;
	static CurveModifierData cmd;
	static LatticeModifierData lmd;
	static ShapeKeyModifierData smd;
	static int init = 1;
	ModifierData *md;

	if (init) {
		md = modifier_new(eModifierType_Armature);
		amd = *((ArmatureModifierData*) md);
		modifier_free(md);

		md = modifier_new(eModifierType_Curve);
		cmd = *((CurveModifierData*) md);
		modifier_free(md);

		md = modifier_new(eModifierType_Lattice);
		lmd = *((LatticeModifierData*) md);
		modifier_free(md);

		md = modifier_new(eModifierType_ShapeKey);
		smd = *((ShapeKeyModifierData*) md);
		modifier_free(md);

		amd.modifier.mode |= eModifierMode_Virtual;
		cmd.modifier.mode |= eModifierMode_Virtual;
		lmd.modifier.mode |= eModifierMode_Virtual;
		smd.modifier.mode |= eModifierMode_Virtual;

		init = 0;
	}

	md = ob->modifiers.first;

	if(ob->parent) {
		if(ob->parent->type==OB_ARMATURE && ob->partype==PARSKEL) {
			amd.object = ob->parent;
			amd.modifier.next = md;
			amd.deformflag= ((bArmature *)(ob->parent->data))->deformflag;
			md = &amd.modifier;
		} else if(ob->parent->type==OB_CURVE && ob->partype==PARSKEL) {
			cmd.object = ob->parent;
			cmd.defaxis = ob->trackflag + 1;
			cmd.modifier.next = md;
			md = &cmd.modifier;
		} else if(ob->parent->type==OB_LATTICE && ob->partype==PARSKEL) {
			lmd.object = ob->parent;
			lmd.modifier.next = md;
			md = &lmd.modifier;
		}
	}

	/* shape key modifier, not yet for curves */
	if(ELEM(ob->type, OB_MESH, OB_LATTICE) && ob_get_key(ob)) {
		if(ob->type == OB_MESH && (ob->shapeflag & OB_SHAPE_EDIT_MODE))
			smd.modifier.mode |= eModifierMode_Editmode|eModifierMode_OnCage;
		else
			smd.modifier.mode &= ~eModifierMode_Editmode|eModifierMode_OnCage;

		smd.modifier.next = md;
		md = &smd.modifier;
	}

	return md;
}
/* Takes an object and returns its first selected armature, else just its
 * armature
 * This should work for multiple armatures per object
 */
Object *modifiers_isDeformedByArmature(Object *ob)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	ArmatureModifierData *amd= NULL;
	
	/* return the first selected armature, this lets us use multiple armatures
	*/
	for (; md; md=md->next) {
		if (md->type==eModifierType_Armature) {
			amd = (ArmatureModifierData*) md;
			if (amd->object && (amd->object->flag & SELECT))
				return amd->object;
		}
	}
	
	if (amd) /* if were still here then return the last armature */
		return amd->object;
	
	return NULL;
}

/* Takes an object and returns its first selected lattice, else just its
* lattice
* This should work for multiple lattics per object
*/
Object *modifiers_isDeformedByLattice(Object *ob)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	LatticeModifierData *lmd= NULL;
	
	/* return the first selected lattice, this lets us use multiple lattices
	*/
	for (; md; md=md->next) {
		if (md->type==eModifierType_Lattice) {
			lmd = (LatticeModifierData*) md;
			if (lmd->object && (lmd->object->flag & SELECT))
				return lmd->object;
		}
	}
	
	if (lmd) /* if were still here then return the last lattice */
		return lmd->object;
	
	return NULL;
}



int modifiers_usesArmature(Object *ob, bArmature *arm)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);

	for (; md; md=md->next) {
		if (md->type==eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData*) md;
			if (amd->object && amd->object->data==arm) 
				return 1;
		}
	}

	return 0;
}

int modifier_isCorrectableDeformed(ModifierData *md)
{
	if (md->type==eModifierType_Armature)
		return 1;
	if (md->type==eModifierType_ShapeKey)
		return 1;
	
	return 0;
}

int modifiers_isCorrectableDeformed(Scene *scene, Object *ob)
{
	ModifierData *md = modifiers_getVirtualModifierList(ob);
	
	for (; md; md=md->next) {
		if(ob->mode==OB_MODE_EDIT && (md->mode & eModifierMode_Editmode)==0);
		else 
			if(modifier_isCorrectableDeformed(md))
				return 1;
	}
	return 0;
}

int modifiers_indexInObject(Object *ob, ModifierData *md_seek)
{
	int i= 0;
	ModifierData *md;
	
	for (md=ob->modifiers.first; (md && md_seek!=md); md=md->next, i++);
	if (!md) return -1; /* modifier isnt in the object */
	return i;
}

void modifier_freeTemporaryData(ModifierData *md)
{
	if(md->type == eModifierType_Armature) {
		ArmatureModifierData *amd= (ArmatureModifierData*)md;

		if(amd->prevCos) {
			MEM_freeN(amd->prevCos);
			amd->prevCos= NULL;
		}
	}
}



