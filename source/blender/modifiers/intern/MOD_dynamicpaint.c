/*
* ***** BEGIN GPL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* Contributor(s): Miika Hämäläinen
*
* ***** END GPL LICENSE BLOCK *****
*
*/

#include "stddef.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_dynamicpaint.h"

#include "depsgraph_private.h"

#include "MOD_util.h"


static void initData(ModifierData *md) 
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData*) md;
	
	pmd->canvas = NULL;
	pmd->brush = NULL;
	pmd->type = MOD_DYNAMICPAINT_TYPE_CANVAS;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	DynamicPaintModifierData *pmd  = (DynamicPaintModifierData*)md;
	DynamicPaintModifierData *tpmd = (DynamicPaintModifierData*)target;
	
	dynamicPaint_Modifier_copy(pmd, tpmd);
}

static void freeData(ModifierData *md)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData*) md;
	dynamicPaint_Modifier_free (pmd);
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	CustomDataMask dataMask = 0;

	dataMask |= (1 << CD_MTFACE);
	dataMask |= (1 << CD_MCOL);
	dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, 
						DerivedMesh *dm,
						int useRenderParams,
						int isFinalCalc)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData*) md;

	return dynamicPaint_Modifier_do(pmd, md->scene, ob, dm);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
						struct Scene *scene,
						Object *ob,
						DagNode *obNode)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData*) md;

	/* add relation from canvases to all brush objects */
	if(pmd && pmd->canvas)
	{
		Base *base = scene->base.first;

		for(; base; base = base->next) {
			DynamicPaintModifierData *pmd2 = (DynamicPaintModifierData *)modifiers_findByType(base->object, eModifierType_DynamicPaint);

			if(pmd2 && pmd2->brush && ob!=base->object)
			{
				DagNode *brushNode = dag_get_node(forest, base->object);
				dag_add_relation(forest, brushNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Dynamic Paint Brush");
			}
		}
	}
}

static int dependsOnTime(ModifierData *md)
{
	return 1;
}

static void foreachIDLink(ModifierData *md, Object *ob,
					   IDWalkFunc walk, void *userData)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData*) md;

	if(pmd->canvas) {
		DynamicPaintSurface *surface = pmd->canvas->surfaces.first;

		for(; surface; surface=surface->next) {
			walk(userData, ob, (ID **)&surface->brush_group);
		}
	}
	if (pmd->brush) {
		walk(userData, ob, (ID **)&pmd->brush->mat);
	}
}

ModifierTypeInfo modifierType_DynamicPaint = {
	/* name */              "Dynamic Paint",
	/* structName */        "DynamicPaintModifierData",
	/* structSize */        sizeof(DynamicPaintModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_UsesPointCache
							| eModifierTypeFlag_Single,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformMatrices */    0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     foreachIDLink,
};
