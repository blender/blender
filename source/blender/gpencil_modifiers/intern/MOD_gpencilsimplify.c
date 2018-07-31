/* ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_gpencilsimplify.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_vec_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
	SimplifyGpencilModifierData *gpmd = (SimplifyGpencilModifierData *)md;
	gpmd->pass_index = 0;
	gpmd->step = 1;
	gpmd->factor = 0.0f;
	gpmd->layername[0] = '\0';
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

static void deformStroke(
        GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
        Object *ob, bGPDlayer *gpl, bGPDstroke *gps)
{
	SimplifyGpencilModifierData *mmd = (SimplifyGpencilModifierData *)md;

	if (!is_stroke_affected_by_modifier(ob,
	        mmd->layername, mmd->pass_index, 4, gpl, gps,
	        mmd->flag & GP_SIMPLIFY_INVERT_LAYER, mmd->flag & GP_SIMPLIFY_INVERT_PASS))
	{
		return;
	}

	if (mmd->mode == GP_SIMPLIFY_FIXED) {
		for (int i = 0; i < mmd->step; i++) {
			BKE_gpencil_simplify_fixed(gps);
		}
	}
	else {
		/* simplify stroke using Ramer-Douglas-Peucker algorithm */
		BKE_gpencil_simplify_stroke(gps, mmd->factor);
	}
}

static void bakeModifier(
		struct Main *UNUSED(bmain), Depsgraph *depsgraph,
        GpencilModifierData *md, Object *ob)
{
	bGPdata *gpd = ob->data;

	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
				deformStroke(md, depsgraph, ob, gpl, gps);
			}
		}
	}
}

GpencilModifierTypeInfo modifierType_Gpencil_Simplify = {
	/* name */              "Simplify",
	/* structName */        "SimplifyGpencilModifierData",
	/* structSize */        sizeof(SimplifyGpencilModifierData),
	/* type */              eGpencilModifierTypeType_Gpencil,
	/* flags */             eGpencilModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,

	/* deformStroke */      deformStroke,
	/* generateStrokes */   NULL,
	/* bakeModifier */    bakeModifier,

	/* initData */          initData,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
