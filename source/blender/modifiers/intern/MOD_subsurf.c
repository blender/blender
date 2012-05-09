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

/** \file blender/modifiers/intern/MOD_subsurf.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"


#include "BKE_cdderivedmesh.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"

#include "MOD_modifiertypes.h"

#include "CCGSubSurf.h"

static void initData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;

	smd->levels = 1;
	smd->renderLevels = 2;
	smd->flags |= eSubsurfModifierFlag_SubsurfUv;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	SubsurfModifierData *tsmd = (SubsurfModifierData *) target;

	tsmd->flags = smd->flags;
	tsmd->levels = smd->levels;
	tsmd->renderLevels = smd->renderLevels;
	tsmd->subdivType = smd->subdivType;
}

static void freeData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;

	if (smd->mCache) {
		ccgSubSurf_free(smd->mCache);
	}
	if (smd->emCache) {
		ccgSubSurf_free(smd->emCache);
	}
}

static int isDisabled(ModifierData *md, int useRenderParams)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	int levels = (useRenderParams) ? smd->renderLevels : smd->levels;

	return get_render_subsurf_level(&md->scene->r, levels) == 0;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  int useRenderParams,
                                  int isFinalCalc)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	DerivedMesh *result;

	result = subsurf_make_derived_from_derived(derivedData, smd,
	                                           useRenderParams, NULL, isFinalCalc, 0, (ob->flag & OB_MODE_EDIT));
	
	if (useRenderParams || !isFinalCalc) {
		DerivedMesh *cddm = CDDM_copy(result);
		result->release(result);
		result = cddm;
	}

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *UNUSED(ob),
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *derivedData)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	DerivedMesh *result;

	result = subsurf_make_derived_from_derived(derivedData, smd, 0,
	                                           NULL, 0, 1, 1);

	return result;
}


ModifierTypeInfo modifierType_Subsurf = {
	/* name */              "Subsurf",
	/* structName */        "SubsurfModifierData",
	/* structSize */        sizeof(SubsurfModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode |
	                        eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};

