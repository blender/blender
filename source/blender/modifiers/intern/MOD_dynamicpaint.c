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
	pmd->paint = NULL;
	pmd->baking = 0;
	pmd->time = -1;
	pmd->type = 0;
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

	return dataMask;
}

static void deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
	  float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData*) md;
	DerivedMesh *dm = get_cddm(ob, NULL, derivedData, vertexCos);


	dynamicPaint_Modifier_do(pmd, md->scene, ob, dm);

	if(dm != derivedData)
		dm->release(dm);
}

static int dependsOnTime(ModifierData *md)
{
	return 1;
}


ModifierTypeInfo modifierType_DynamicPaint = {
	/* name */              "Dynamic Paint",
	/* structName */        "DynamicPaintModifierData",
	/* structSize */        sizeof(DynamicPaintModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_Single,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
