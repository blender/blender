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
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"
#include "BKE_utildefines.h"
#include "BKE_tessmesh.h"

#include "MEM_guardedalloc.h"
#include "depsgraph_private.h"

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
	tmmd->mirror_ob = mmd->mirror_ob;;
}

static void foreachObjectLink(
						 ModifierData *md, Object *ob,
	  void (*walk)(void *userData, Object *ob, Object **obpoin),
		 void *userData)
{
	MirrorModifierData *mmd = (MirrorModifierData*) md;

	walk(userData, ob, &mmd->mirror_ob);
}

static void updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *scene,
					  Object *ob, DagNode *obNode)
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

void vertgroup_flip_name (char *name, int strip_number);
DerivedMesh *doMirrorOnAxis(MirrorModifierData *mmd,
		Object *ob,
		DerivedMesh *dm,
		int initFlags,
		int axis)
{
	float tolerance = mmd->tolerance;
	DerivedMesh *result, *cddm;
	BMEditMesh *em;
	BMesh *bm;
	BMOIter siter1;
	BMOperator op;
	BMVert *v1;
	BMIter iter;
	bDeformGroup *def, *defb;
	bDeformGroup **vector_def = NULL;
	float mtx[4][4], imtx[4][4];
	int j;
	int vector_size=0, a, b;

	cddm = dm; //copying shouldn't be necassary here, as all modifiers return CDDM's
	em = CDDM_To_BMesh(dm, NULL);

	/*convienence variable*/
	bm = em->bm;

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

	if (mmd->mirror_ob) {
		float mtx2[4][4];

		invert_m4_m4(mtx2, mmd->mirror_ob->obmat);
		mul_m4_m4m4(mtx, ob->obmat, mtx2);
	} else {
		unit_m4(mtx);
	}

	BMO_InitOpf(bm, &op, "mirror geom=%avef mat=%m4 mergedist=%f axis=%d",
				mtx, mmd->tolerance, axis);

	BMO_Exec_Op(bm, &op);

	BMO_CallOpf(bm, "reversefaces faces=%s", &op, "newout");
	
	/*handle vgroup stuff*/
	if (mmd->flag & MOD_MIR_VGROUP) {
		BMO_ITER(v1, &siter1, bm, &op, "newout", BM_VERT) {
			MDeformVert *dvert = CustomData_bmesh_get(&bm->vdata, v1->head.data, CD_MDEFORMVERT);

			if (dvert) {
				for(j = 0; j < dvert[0].totweight; ++j) {
					char tmpname[32];

					if(dvert->dw[j].def_nr < 0 ||
					   dvert->dw[j].def_nr >= vector_size)
						continue;

					def = vector_def[dvert->dw[j].def_nr];
					strcpy(tmpname, def->name);
					vertgroup_flip_name(tmpname,0);

					for(b = 0, defb = ob->defbase.first; defb;
						defb = defb->next, b++)
					{
						if(!strcmp(defb->name, tmpname))
						{
							dvert->dw[j].def_nr = b;
							break;
						}
					}
				}
			}
		}
	}

	BMO_Finish_Op(bm, &op);

	if (vector_def) MEM_freeN(vector_def);
	
	BMEdit_RecalcTesselation(em);
	result = CDDM_from_BMEditMesh(em, NULL);

	BMEdit_Free(em);
	MEM_freeN(em);
		
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

static DerivedMesh *applyModifier(
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

static DerivedMesh *applyModifierEM(
		ModifierData *md, Object *ob, struct EditMesh *editData,
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
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
