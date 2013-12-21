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

#include "intern/CCGSubSurf.h"

static void initData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;

	smd->levels = 1;
	smd->renderLevels = 2;
	smd->flags |= eSubsurfModifierFlag_SubsurfUv;
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
#endif
	SubsurfModifierData *tsmd = (SubsurfModifierData *) target;

	modifier_copyData_generic(md, target);

	tsmd->emCache = tsmd->mCache = NULL;

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

static bool isDisabled(ModifierData *md, int useRenderParams)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	int levels = (useRenderParams) ? smd->renderLevels : smd->levels;

	return get_render_subsurf_level(&md->scene->r, levels) == 0;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag flag)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	SubsurfFlags subsurf_flags = 0;
	DerivedMesh *result;
	const int useRenderParams = flag & MOD_APPLY_RENDER;
	const int isFinalCalc = flag & MOD_APPLY_USECACHE;

	if (useRenderParams)
		subsurf_flags |= SUBSURF_USE_RENDER_PARAMS;
	if (isFinalCalc)
		subsurf_flags |= SUBSURF_IS_FINAL_CALC;
	if (ob->mode & OB_MODE_EDIT)
		subsurf_flags |= SUBSURF_IN_EDIT_MODE;
	
	result = subsurf_make_derived_from_derived(derivedData, smd, NULL, subsurf_flags);
	result->cd_flag = derivedData->cd_flag;
	
	if (useRenderParams || !isFinalCalc) {
		DerivedMesh *cddm = CDDM_copy(result);
		result->release(result);
		result = cddm;
	}

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *UNUSED(ob),
                                    struct BMEditMesh *UNUSED(editData),
                                    DerivedMesh *derivedData,
                                    ModifierApplyFlag flag)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	DerivedMesh *result;
	/* 'orco' using editmode flags would cause cache to be used twice in editbmesh_calc_modifiers */
	SubsurfFlags ss_flags = (flag & MOD_APPLY_ORCO) ? 0 : (SUBSURF_FOR_EDIT_MODE | SUBSURF_IN_EDIT_MODE);

	result = subsurf_make_derived_from_derived(derivedData, smd, NULL, ss_flags);

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

