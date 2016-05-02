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

/** \file blender/modifiers/intern/MOD_explode.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"


#include "MEM_guardedalloc.h"


static void initData(ModifierData *md)
{
	ExplodeModifierData *emd = (ExplodeModifierData *) md;

	emd->facepa = NULL;
	emd->flag |= eExplodeFlag_Unborn + eExplodeFlag_Alive + eExplodeFlag_Dead;
}
static void freeData(ModifierData *md)
{
	ExplodeModifierData *emd = (ExplodeModifierData *) md;
	
	if (emd->facepa) MEM_freeN(emd->facepa);
}
static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	ExplodeModifierData *emd = (ExplodeModifierData *) md;
#endif
	ExplodeModifierData *temd = (ExplodeModifierData *) target;

	modifier_copyData_generic(md, target);

	temd->facepa = NULL;
}
static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}
static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	ExplodeModifierData *emd = (ExplodeModifierData *) md;
	CustomDataMask dataMask = 0;

	if (emd->vgroup)
		dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *UNUSED(md), Object *UNUSED(ob),
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	return derivedData;
}


ModifierTypeInfo modifierType_Explode = {
	/* name */              "Explode",
	/* structName */        "ExplodeModifierData",
	/* structSize */        sizeof(ExplodeModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh,
	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
