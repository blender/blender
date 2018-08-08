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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpencil_modifiers/intern/MOD_gpencil_util.c
 *  \ingroup bke
 */


#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_math_color.h"
#include "BLI_rand.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_colortools.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_util.h"

void gpencil_modifier_type_init(GpencilModifierTypeInfo *types[])
{
#define INIT_GP_TYPE(typeName) (types[eGpencilModifierType_##typeName] = &modifierType_Gpencil_##typeName)
	INIT_GP_TYPE(Noise);
	INIT_GP_TYPE(Subdiv);
	INIT_GP_TYPE(Simplify);
	INIT_GP_TYPE(Thick);
	INIT_GP_TYPE(Tint);
	INIT_GP_TYPE(Color);
	INIT_GP_TYPE(Instance);
	INIT_GP_TYPE(Build);
	INIT_GP_TYPE(Opacity);
	INIT_GP_TYPE(Lattice);
	INIT_GP_TYPE(Mirror);
	INIT_GP_TYPE(Smooth);
	INIT_GP_TYPE(Hook);
	INIT_GP_TYPE(Offset);
#undef INIT_GP_TYPE
}

/* verify if valid layer and pass index */
bool is_stroke_affected_by_modifier(
        Object *ob, char *mlayername, int mpassindex, int minpoints,
        bGPDlayer *gpl, bGPDstroke *gps, bool inv1, bool inv2)
{
	MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);

	/* omit if filter by layer */
	if (mlayername[0] != '\0') {
		if (inv1 == false) {
			if (!STREQ(mlayername, gpl->info)) {
				return false;
			}
		}
		else {
			if (STREQ(mlayername, gpl->info)) {
				return false;
			}
		}
	}
	/* verify pass */
	if (mpassindex > 0) {
		if (inv2 == false) {
			if (gp_style->index != mpassindex) {
				return false;
			}
		}
		else {
			if (gp_style->index == mpassindex) {
				return false;
			}
		}
	}
	/* need to have a minimum number of points */
	if ((minpoints > 0) && (gps->totpoints < minpoints)) {
		return false;
	}

	return true;
}

/* verify if valid vertex group *and return weight */
float get_modifier_point_weight(MDeformVert *dvert, int inverse, int vindex)
{
	float weight = 1.0f;

	if (vindex >= 0) {
		weight = BKE_gpencil_vgroup_use_index(dvert, vindex);
		if ((weight >= 0.0f) && (inverse == 1)) {
			return -1.0f;
		}

		if ((weight < 0.0f) && (inverse == 0)) {
			return -1.0f;
		}

		/* if inverse, weight is always 1 */
		if ((weight < 0.0f) && (inverse == 1)) {
			return 1.0f;
		}

	}

	return weight;
}

/* set material when apply modifiers (used in tint and color modifier) */
void gpencil_apply_modifier_material(
	Main *bmain, Object *ob, Material *mat,
	GHash *gh_color, bGPDstroke *gps, bool crt_material)
{
	MaterialGPencilStyle *gp_style = mat->gp_style;

	/* look for color */
	if (crt_material) {
		Material *newmat = BLI_ghash_lookup(gh_color, mat->id.name);
		if (newmat == NULL) {
			BKE_object_material_slot_add(bmain, ob);
			newmat = BKE_material_copy(bmain, mat);
			newmat->preview = NULL;

			assign_material(bmain, ob, newmat, ob->totcol, BKE_MAT_ASSIGN_USERPREF);

			copy_v4_v4(newmat->gp_style->stroke_rgba, gps->runtime.tmp_stroke_rgba);
			copy_v4_v4(newmat->gp_style->fill_rgba, gps->runtime.tmp_fill_rgba);

			BLI_ghash_insert(gh_color, newmat->id.name, newmat);
			DEG_id_tag_update(&newmat->id, DEG_TAG_COPY_ON_WRITE);
		}
		/* reasign color index */
		int idx = BKE_gpencil_get_material_index(ob, newmat);
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
