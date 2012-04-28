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

/** \file blender/modifiers/intern/MOD_boolean.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"

#include "depsgraph_private.h"

#include "MOD_boolean_util.h"
#include "MOD_util.h"

#include "PIL_time.h"

static void copyData(ModifierData *md, ModifierData *target)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;
	BooleanModifierData *tbmd = (BooleanModifierData*) target;

	tbmd->object = bmd->object;
	tbmd->operation = bmd->operation;
}

static int isDisabled(ModifierData *md, int UNUSED(useRenderParams))
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

static void updateDepgraph(ModifierData *md, DagForest *forest,
						struct Scene *UNUSED(scene),
						Object *UNUSED(ob),
						DagNode *obNode)
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;

	if (bmd->object) {
		DagNode *curNode = dag_get_node(forest, bmd->object);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Boolean Modifier");
	}
}

#ifdef WITH_MOD_BOOLEAN
static DerivedMesh *get_quick_derivedMesh(DerivedMesh *derivedData, DerivedMesh *dm, int operation)
{
	DerivedMesh *result = NULL;

	if (derivedData->getNumPolys(derivedData) == 0 || dm->getNumPolys(dm) == 0) {
		switch (operation) {
			case eBooleanModifierOp_Intersect:
				result = CDDM_new(0, 0, 0, 0, 0);
				break;

			case eBooleanModifierOp_Union:
				if (derivedData->getNumPolys(derivedData)) result = derivedData;
				else result = CDDM_copy(dm);

				break;

			case eBooleanModifierOp_Difference:
				result = derivedData;
				break;
		}
	}

	return result;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	BooleanModifierData *bmd = (BooleanModifierData*) md;
	DerivedMesh *dm;

	if (!bmd->object)
		return derivedData;

	dm = bmd->object->derivedFinal;

	if (dm) {
		DerivedMesh *result;

		/* when one of objects is empty (has got no faces) we could speed up
		 * calculation a bit returning one of objects' derived meshes (or empty one)
		 * Returning mesh is depended on modifiers operation (sergey) */
		result = get_quick_derivedMesh(derivedData, dm, bmd->operation);

		if (result == NULL) {

			DM_ensure_tessface(dm);          /* BMESH - UNTIL MODIFIER IS UPDATED FOR MPoly */
			DM_ensure_tessface(derivedData); /* BMESH - UNTIL MODIFIER IS UPDATED FOR MPoly */

			// TIMEIT_START(NewBooleanDerivedMesh)

			result = NewBooleanDerivedMesh(dm, bmd->object, derivedData, ob,
					1 + bmd->operation);

			// TIMEIT_END(NewBooleanDerivedMesh)
		}

		/* if new mesh returned, return it; otherwise there was
		 * an error, so delete the modifier object */
		if (result)
			return result;
		else
			modifier_setError(md, "%s", TIP_("Can't execute boolean operation."));
	}
	
	return derivedData;
}
#else // WITH_MOD_BOOLEAN
static DerivedMesh *applyModifier(ModifierData *UNUSED(md), Object *UNUSED(ob),
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	return derivedData;
}
#endif // WITH_MOD_BOOLEAN

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *UNUSED(md))
{
	CustomDataMask dataMask = CD_MASK_MTFACE | CD_MASK_MEDGE;

	dataMask |= CD_MASK_MDEFORMVERT;
	
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
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          NULL,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
