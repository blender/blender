/*
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
 */

/** \file \ingroup modifiers
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BLI_utildefines.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
	SubdivGpencilModifierData *gpmd = (SubdivGpencilModifierData *)md;
	gpmd->pass_index = 0;
	gpmd->level = 1;
	gpmd->layername[0] = '\0';
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

/* subdivide stroke to get more control points */
static void deformStroke(
	GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
	Object *ob, bGPDlayer *gpl, bGPDstroke *gps)
{
	SubdivGpencilModifierData *mmd = (SubdivGpencilModifierData *)md;

	if (!is_stroke_affected_by_modifier(
	            ob,
	            mmd->layername, mmd->pass_index, mmd->layer_pass, 3, gpl, gps,
	            mmd->flag & GP_SUBDIV_INVERT_LAYER, mmd->flag & GP_SUBDIV_INVERT_PASS,
	            mmd->flag & GP_SUBDIV_INVERT_LAYERPASS))
	{
		return;
	}

	BKE_gpencil_subdivide(gps, mmd->level, mmd->flag);
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

static int getDuplicationFactor(GpencilModifierData *md)
{
	SubdivGpencilModifierData *mmd = (SubdivGpencilModifierData *)md;
	int t = (mmd->level + 1) * (mmd->level + 1);
	CLAMP_MIN(t, 2);
	return t;
}

GpencilModifierTypeInfo modifierType_Gpencil_Subdiv = {
	/* name */              "Subdivision",
	/* structName */        "SubdivGpencilModifierData",
	/* structSize */        sizeof(SubdivGpencilModifierData),
	/* type */              eGpencilModifierTypeType_Gpencil,
	/* flags */             eGpencilModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,

	/* deformStroke */      deformStroke,
	/* generateStrokes */   NULL,
	/* bakeModifier */      bakeModifier,
	/* remapTime */         NULL,

	/* initData */          initData,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
	/* getDuplicationFactor */ getDuplicationFactor,
};
