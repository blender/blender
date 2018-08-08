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

/** \file blender/gpencil_modifiers/intern/MOD_gpencilopacity.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_material.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
	OpacityGpencilModifierData *gpmd = (OpacityGpencilModifierData *)md;
	gpmd->pass_index = 0;
	gpmd->factor = 1.0f;
	gpmd->layername[0] = '\0';
	gpmd->vgname[0] = '\0';
	gpmd->flag |= GP_OPACITY_CREATE_COLORS;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

/* opacity strokes */
static void deformStroke(
        GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
        Object *ob, bGPDlayer *gpl, bGPDstroke *gps)
{
	OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;
	int vindex = defgroup_name_index(ob, mmd->vgname);

	if (!is_stroke_affected_by_modifier(
	            ob,
	            mmd->layername, mmd->pass_index, 1, gpl, gps,
	            mmd->flag & GP_OPACITY_INVERT_LAYER, mmd->flag & GP_OPACITY_INVERT_PASS))
	{
		return;
	}

	gps->runtime.tmp_fill_rgba[3] *= mmd->factor;

	/* if factor is > 1, then force opacity */
	if (mmd->factor > 1.0f) {
		gps->runtime.tmp_stroke_rgba[3] += mmd->factor - 1.0f;
		if (gps->runtime.tmp_fill_rgba[3] > 1e-5) {
			gps->runtime.tmp_fill_rgba[3] += mmd->factor - 1.0f;
		}
	}

	CLAMP(gps->runtime.tmp_stroke_rgba[3], 0.0f, 1.0f);
	CLAMP(gps->runtime.tmp_fill_rgba[3], 0.0f, 1.0f);

	/* if opacity > 1.0, affect the strength of the stroke */
	if (mmd->factor > 1.0f) {
		for (int i = 0; i < gps->totpoints; i++) {
			bGPDspoint *pt = &gps->points[i];
			MDeformVert *dvert = &gps->dvert[i];

			/* verify vertex group */
			float weight = get_modifier_point_weight(dvert, ((mmd->flag & GP_OPACITY_INVERT_VGROUP) != 0), vindex);
			if (weight < 0) {
				pt->strength += mmd->factor - 1.0f;
			}
			else {
				pt->strength += (mmd->factor - 1.0f) * weight;
			}
			CLAMP(pt->strength, 0.0f, 1.0f);
		}
	}
}

static void bakeModifier(
	Main *bmain, Depsgraph *depsgraph,
	GpencilModifierData *md, Object *ob)
{
	OpacityGpencilModifierData *mmd = (OpacityGpencilModifierData *)md;
	bGPdata *gpd = ob->data;

	GHash *gh_color = BLI_ghash_str_new("GP_Opacity modifier");
	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {

				Material *mat = give_current_material(ob, gps->mat_nr + 1);
				if (mat == NULL)
					continue;
				MaterialGPencilStyle *gp_style = mat->gp_style;
				/* skip stroke if it doesn't have color info */
				if (ELEM(NULL, gp_style))
					continue;

				copy_v4_v4(gps->runtime.tmp_stroke_rgba, gp_style->stroke_rgba);
				copy_v4_v4(gps->runtime.tmp_fill_rgba, gp_style->fill_rgba);

				deformStroke(md, depsgraph, ob, gpl, gps);

				gpencil_apply_modifier_material(bmain, ob, mat, gh_color, gps,
					(bool)(mmd->flag & GP_OPACITY_CREATE_COLORS));
			}
		}
	}
	/* free hash buffers */
	if (gh_color) {
		BLI_ghash_free(gh_color, NULL, NULL);
		gh_color = NULL;
	}
}

GpencilModifierTypeInfo modifierType_Gpencil_Opacity = {
	/* name */              "Opacity",
	/* structName */        "OpacityGpencilModifierData",
	/* structSize */        sizeof(OpacityGpencilModifierData),
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
