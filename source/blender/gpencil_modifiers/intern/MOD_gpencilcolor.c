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

/** \file blender/gpencil_modifiers/intern/MOD_gpencilcolor.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
	ColorGpencilModifierData *gpmd = (ColorGpencilModifierData *)md;
	gpmd->pass_index = 0;
	ARRAY_SET_ITEMS(gpmd->hsv, 1.0f, 1.0f, 1.0f);
	gpmd->layername[0] = '\0';
	gpmd->flag |= GP_COLOR_CREATE_COLORS;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

/* color correction strokes */
static void deformStroke(
        GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
        Object *ob, bGPDlayer *gpl, bGPDstroke *gps)
{

	ColorGpencilModifierData *mmd = (ColorGpencilModifierData *)md;
	float hsv[3], factor[3];

	if (!is_stroke_affected_by_modifier(ob,
	        mmd->layername, mmd->pass_index, 1, gpl, gps,
	        mmd->flag & GP_COLOR_INVERT_LAYER, mmd->flag & GP_COLOR_INVERT_PASS))
	{
		return;
	}

	copy_v3_v3(factor, mmd->hsv);
	add_v3_fl(factor, -1.0f);

	rgb_to_hsv_v(gps->runtime.tmp_stroke_rgba, hsv);
	add_v3_v3(hsv, factor);
	CLAMP3(hsv, 0.0f, 1.0f);
	hsv_to_rgb_v(hsv, gps->runtime.tmp_stroke_rgba);

	rgb_to_hsv_v(gps->runtime.tmp_fill_rgba, hsv);
	add_v3_v3(hsv, factor);
	CLAMP3(hsv, 0.0f, 1.0f);
	hsv_to_rgb_v(hsv, gps->runtime.tmp_fill_rgba);
}

static void bakeModifier(
        Main *bmain, Depsgraph *depsgraph,
        GpencilModifierData *md, Object *ob)
{
	ColorGpencilModifierData *mmd = (ColorGpencilModifierData *)md;
	bGPdata *gpd = ob->data;

	GHash *gh_color = BLI_ghash_str_new("GP_Color modifier");
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

				/* look for color */
				if (mmd->flag & GP_COLOR_CREATE_COLORS) {
					Material *newmat = BLI_ghash_lookup(gh_color, mat->id.name);
					if (newmat == NULL) {
						BKE_object_material_slot_add(bmain, ob);
						newmat = BKE_material_copy(bmain, mat);
						newmat->preview = NULL;

						assign_material(bmain, ob, newmat, ob->totcol, BKE_MAT_ASSIGN_USERPREF);

						copy_v4_v4(newmat->gp_style->stroke_rgba, gps->runtime.tmp_stroke_rgba);
						copy_v4_v4(newmat->gp_style->fill_rgba, gps->runtime.tmp_fill_rgba);

						BLI_ghash_insert(gh_color, mat->id.name, newmat);
						DEG_id_tag_update(&newmat->id, DEG_TAG_COPY_ON_WRITE);
					}
					/* reasign color index */
					int idx = BKE_object_material_slot_find_index(ob, newmat);
					gps->mat_nr = idx - 1;
				}
				else {
					/* reuse existing color (but update only first time) */
					if (BLI_ghash_lookup(gh_color, mat->id.name) == NULL) {
						copy_v4_v4(gp_style->stroke_rgba, gps->runtime.tmp_stroke_rgba);
						copy_v4_v4(gp_style->fill_rgba, gps->runtime.tmp_fill_rgba);
						BLI_ghash_insert(gh_color, mat->id.name, mat);
					}
					/* update previews (icon and thumbnail) */
					if (mat->preview != NULL) {
						mat->preview->flag[ICON_SIZE_ICON] |= PRV_CHANGED;
						mat->preview->flag[ICON_SIZE_PREVIEW] |= PRV_CHANGED;
					}
					DEG_id_tag_update(&mat->id, DEG_TAG_COPY_ON_WRITE);
				}

			}
		}
	}
	/* free hash buffers */
	if (gh_color) {
		BLI_ghash_free(gh_color, NULL, NULL);
		gh_color = NULL;
	}
}

GpencilModifierTypeInfo modifierType_Gpencil_Color = {
	/* name */              "Hue/Saturation",
	/* structName */        "ColorGpencilModifierData",
	/* structSize */        sizeof(ColorGpencilModifierData),
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
