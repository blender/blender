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


#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"

#include "depsgraph_private.h"

#include "MOD_boolean_util.h"


static void copyData(ModifierData *md, ModifierData *target)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;
	BooleanModifierData *tbmd = (BooleanModifierData*) target;

	tbmd->object = bmd->object;
	tbmd->operation = bmd->operation;
}

static int isDisabled(ModifierData *md, int useRenderParams)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;

	return !bmd->object;
}

static void foreachObjectLink(
						  ModifierData *md, Object *ob,
	   void (*walk)(void *userData, Object *ob, Object **obpoin),
		  void *userData)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;

	walk(userData, ob, &bmd->object);
}

static void updateDepgraph(
					   ModifierData *md, DagForest *forest, struct Scene *scene, Object *ob,
	DagNode *obNode)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;

	if(bmd->object) {
		DagNode *curNode = dag_get_node(forest, bmd->object);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Boolean Modifier");
	}
}


static DerivedMesh *applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;
	DerivedMesh *dm = bmd->object->derivedFinal;

	/* we do a quick sanity check */
	if(dm && (derivedData->getNumFaces(derivedData) > 3)
			&& bmd->object && dm->getNumFaces(dm) > 3) {
		DerivedMesh *result = NewBooleanDerivedMesh(dm, bmd->object, derivedData, ob,
				1 + bmd->operation);

		/* if new mesh returned, return it; otherwise there was
		* an error, so delete the modifier object */
		if(result)
			return result;
		else
			modifier_setError(md, "Can't execute boolean operation.");
	}
	
	return derivedData;
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	CustomDataMask dataMask = (1 << CD_MTFACE) + (1 << CD_MEDGE);

	dataMask |= (1 << CD_MDEFORMVERT);
	
	return dataMask;
}


ModifierTypeInfo modifierType_Boolean = {
	/* name */              "Boolean",
	/* structName */        "BooleanModifierData",
	/* structSize */        sizeof(BooleanModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_UsesPointCache,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   0,
	/* initData */          0,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          0,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
