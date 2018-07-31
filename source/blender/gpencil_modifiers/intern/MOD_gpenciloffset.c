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
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_gpenciloffset.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
	OffsetGpencilModifierData *gpmd = (OffsetGpencilModifierData *)md;
	gpmd->pass_index = 0;
	gpmd->layername[0] = '\0';
	gpmd->vgname[0] = '\0';
	ARRAY_SET_ITEMS(gpmd->loc, 0.0f, 0.0f, 0.0f);
	ARRAY_SET_ITEMS(gpmd->rot, 0.0f, 0.0f, 0.0f);
	ARRAY_SET_ITEMS(gpmd->scale, 0.0f, 0.0f, 0.0f);
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

/* change stroke offsetness */
static void deformStroke(
        GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
        Object *ob, bGPDlayer *gpl, bGPDstroke *gps)
{
	OffsetGpencilModifierData *mmd = (OffsetGpencilModifierData *)md;
	int vindex = defgroup_name_index(ob, mmd->vgname);

	float mat[4][4];
	float loc[3], rot[3], scale[3];

	if (!is_stroke_affected_by_modifier(ob,
	        mmd->layername, mmd->pass_index, 1, gpl, gps,
	        mmd->flag & GP_OFFSET_INVERT_LAYER, mmd->flag & GP_OFFSET_INVERT_PASS))
	{
		return;
	}

	for (int i = 0; i < gps->totpoints; i++) {
		bGPDspoint *pt = &gps->points[i];
		MDeformVert *dvert = &gps->dvert[i];

		/* verify vertex group */
		float weight = get_modifier_point_weight(dvert, (int)(!(mmd->flag & GP_OFFSET_INVERT_VGROUP) == 0), vindex);
		if (weight < 0) {
			continue;
		}
		/* calculate matrix */
		mul_v3_v3fl(loc, mmd->loc, weight);
		mul_v3_v3fl(rot, mmd->rot, weight);
		mul_v3_v3fl(scale, mmd->scale, weight);
		add_v3_fl(scale, 1.0);
		loc_eul_size_to_mat4(mat, loc, rot, scale);

		mul_m4_v3(mat, &pt->x);
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

GpencilModifierTypeInfo modifierType_Gpencil_Offset = {
	/* name */              "Offset",
	/* structName */        "OffsetGpencilModifierData",
	/* structSize */        sizeof(OffsetGpencilModifierData),
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
