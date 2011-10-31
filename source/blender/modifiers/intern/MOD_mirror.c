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

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_smallhash.h"
#include "BLI_array.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"
#include "BKE_utildefines.h"
#include "BKE_tessmesh.h"

#include "depsgraph_private.h"

/*from MOD_array.c*/
void vertgroup_flip_name (char *name, int strip_number);

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

	tmmd->axis = mmd->axis;
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
	
	if (mmd->mirror_ob)
		walk(userData, ob, &mmd->mirror_ob);
}

static void updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *UNUSED(scene),
					  Object *UNUSED(ob), DagNode *obNode)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	if(mmd->mirror_ob) {
		DagNode *latNode = dag_get_node(forest, mmd->mirror_ob);

		dag_add_relation(forest, latNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Mirror Modifier");
	}
}


/* Mirror */
#define VERT_NEW	1

DerivedMesh *doMirrorOnAxis(MirrorModifierData *mmd,
		Object *ob,
		DerivedMesh *dm,
		int UNUSED(initFlags),
		int axis)
{
	float tolerance_sq;
	DerivedMesh *cddm, *origdm;
	bDeformGroup *def;
	bDeformGroup **vector_def = NULL;
	MVert *mv, *ov;
	MEdge *me;
	MLoop *ml;
	MPoly *mp;
	float mtx[4][4];
	int i, j, *vtargetmap = NULL;
	BLI_array_declare(vtargetmap);
	int vector_size=0, a, totshape;

	tolerance_sq = mmd->tolerance * mmd->tolerance;
	
	origdm = dm;
	if (!CDDM_Check(dm))
		dm = CDDM_copy(dm, 0);
	
	if (mmd->flag & MOD_MIR_VGROUP) {
		/* calculate the number of deformedGroups */
		for(vector_size = 0, def = ob->defbase.first; def;
			def = def->next, vector_size++);

		/* load the deformedGroups for fast access */
		vector_def =
			(bDeformGroup **)MEM_mallocN(sizeof(bDeformGroup*) * vector_size,
										 "group_index");
		for(a = 0, def = ob->defbase.first; def; def = def->next, a++) {
			vector_def[a] = def;
		}
	}

	/*mtx is the mirror transformation*/
	unit_m4(mtx);
	mtx[axis][axis] = -1.0;

	if (mmd->mirror_ob) {
		float tmp[4][4];
		float itmp[4][4];

		/*tmp is a transform from coords relative to the object's own origin, to
		  coords relative to the mirror object origin*/
		invert_m4_m4(tmp, mmd->mirror_ob->obmat);
		mul_m4_m4m4(tmp, ob->obmat, tmp);

		/*itmp is the reverse transform back to origin-relative coordiantes*/
		invert_m4_m4(itmp, tmp);

		/*combine matrices to get a single matrix that translates coordinates into
		  mirror-object-relative space, does the mirror, and translates back to
		  origin-relative space*/
		mul_m4_m4m4(mtx, tmp, mtx);
		mul_m4_m4m4(mtx, mtx, itmp);
	}
	
	cddm = CDDM_from_template(dm, dm->numVertData*2, dm->numEdgeData*2, 0, dm->numLoopData*2, dm->numPolyData*2);
	
	/*copy customdata to original geometry*/
	CustomData_copy_data(&dm->vertData, &cddm->vertData, 0, 0, dm->numVertData);
	CustomData_copy_data(&dm->edgeData, &cddm->edgeData, 0, 0, dm->numEdgeData);
	CustomData_copy_data(&dm->loopData, &cddm->loopData, 0, 0, dm->numLoopData);
	CustomData_copy_data(&dm->polyData, &cddm->polyData, 0, 0, dm->numPolyData);

	/*copy customdata to new geometry*/
	CustomData_copy_data(&dm->vertData, &cddm->vertData, 0, dm->numVertData, dm->numVertData);
	CustomData_copy_data(&dm->edgeData, &cddm->edgeData, 0, dm->numEdgeData, dm->numEdgeData);
	CustomData_copy_data(&dm->polyData, &cddm->polyData, 0, dm->numPolyData, dm->numPolyData);
	
	/*mirror vertex coordinates*/
	ov = CDDM_get_verts(cddm);
	mv = ov + dm->numVertData;
	for (i=0; i<dm->numVertData; i++, mv++, ov++) {
		mul_m4_v3(mtx, mv->co);
		/*compare location of the original and mirrored vertex, to see if they
		  should be mapped for merging*/
		if (len_squared_v3v3(ov->co, mv->co) < tolerance_sq) {
			BLI_array_append(vtargetmap, i+dm->numVertData);
		}
		else {
			BLI_array_append(vtargetmap, -1);
		}
	}
	
	/*handle shape keys*/
	totshape = CustomData_number_of_layers(&cddm->vertData, CD_SHAPEKEY);
	for (a=0; a<totshape; a++) {
		float (*cos)[3] = CustomData_get_layer_n(&cddm->vertData, CD_SHAPEKEY, a);
		for (i=dm->numVertData; i<cddm->numVertData; i++) {
			mul_m4_v3(mtx, cos[i]);
		}
	}
	
	for (i=0; i<dm->numVertData; i++) {
		BLI_array_append(vtargetmap, -1);
	}
	
	/*adjust mirrored edge vertex indices*/
	me = CDDM_get_edges(cddm) + dm->numEdgeData;
	for (i=0; i<dm->numEdgeData; i++, me++) {
		me->v1 += dm->numVertData;
		me->v2 += dm->numVertData;
	}
	
	/*adjust mirrored poly loopstart indices, and reverse loop order (normals)*/	
	mp = CDDM_get_polys(cddm) + dm->numPolyData;
	ml = CDDM_get_loops(cddm);
	for (i=0; i<dm->numPolyData; i++, mp++) {
		MLoop *ml2;
		int e;
		
		for (j=0; j<mp->totloop; j++) {
			CustomData_copy_data(&dm->loopData, &cddm->loopData, mp->loopstart+j,
								 mp->loopstart+dm->numLoopData+mp->totloop-j-1, 1);
		}
		
		ml2 = ml + mp->loopstart + dm->numLoopData;
		e = ml2[0].e;
		for (j=0; j<mp->totloop-1; j++) {
			ml2[j].e = ml2[j+1].e;
		}
		ml2[mp->totloop-1].e = e;
		
		mp->loopstart += dm->numLoopData;
	}

	/*adjust mirrored loop vertex and edge indices*/	
	ml = CDDM_get_loops(cddm) + dm->numLoopData;
	for (i=0; i<dm->numLoopData; i++, ml++) {
		ml->v += dm->numVertData;
		ml->e += dm->numEdgeData;
	}

	CDDM_recalc_tesselation(cddm, 1);
	
	/*handle vgroup stuff*/
	if ((mmd->flag & MOD_MIR_VGROUP) && CustomData_has_layer(&cddm->vertData, CD_MDEFORMVERT)) {
		MDeformVert *dvert = CustomData_get_layer(&cddm->vertData, CD_MDEFORMVERT);
		int *flip_map= NULL, flip_map_len= 0;

		flip_map= defgroup_flip_map(ob, &flip_map_len, FALSE);
		
		for (i=0; i<dm->numVertData; i++, dvert++) {
			defvert_flip(dvert, flip_map, flip_map_len);
		}
	}
	
	if (!(mmd->flag & MOD_MIR_NO_MERGE))
		cddm = CDDM_merge_verts(cddm, vtargetmap);
	
	BLI_array_free(vtargetmap);
	
	if (vector_def) MEM_freeN(vector_def);
	
	if (dm != origdm) {
		dm->needsFree = 1;
		dm->release(dm);
	}
	
	return cddm;
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

static DerivedMesh *applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int UNUSED(useRenderParams), int UNUSED(isFinalCalc))
{
	DerivedMesh *result;
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	result = mirrorModifier__doMirror(mmd, ob, derivedData, 0);

	if(result != derivedData)
		CDDM_calc_normals(result);
	
	return result;
}

static DerivedMesh *applyModifierEM(
		ModifierData *md, Object *ob, struct BMEditMesh *UNUSED(editData),
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
