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
 * Contributor(s): Antonio Vazquez, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/gpencil_modifiers/intern/MOD_gpencilinstance.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_layer.h"
#include "BKE_collection.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
	InstanceGpencilModifierData *gpmd = (InstanceGpencilModifierData *)md;
	gpmd->count[0] = 1;
	gpmd->count[1] = 1;
	gpmd->count[2] = 1;
	gpmd->offset[0] = 1.0f;
	gpmd->offset[1] = 1.0f;
	gpmd->offset[2] = 1.0f;
	gpmd->shift[0] = 0.0f;
	gpmd->shift[1] = 0.0f;
	gpmd->shift[2] = 0.0f;
	gpmd->scale[0] = 1.0f;
	gpmd->scale[1] = 1.0f;
	gpmd->scale[2] = 1.0f;
	gpmd->rnd_rot = 0.5f;
	gpmd->rnd_size = 0.5f;
	gpmd->lock_axis |= GP_LOCKAXIS_X;

	/* fill random values */
	BLI_array_frand(gpmd->rnd, 20, 1);
	gpmd->rnd[0] = 1;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

/* -------------------------------- */

/* array modifier - generate geometry callback (for viewport/rendering) */
/* TODO: How to skip this for the simplify options?   -->  !GP_SIMPLIFY_MODIF(ts, playing) */
static void generate_geometry(
        GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
        Object *ob, bGPDlayer *gpl, bGPDframe *gpf)
{
	InstanceGpencilModifierData *mmd = (InstanceGpencilModifierData *)md;
	ListBase stroke_cache = {NULL, NULL};
	bGPDstroke *gps;
	int idx;

	/* Check which strokes we can use once, and store those results in an array
	 * for quicker checking of what's valid (since string comparisons are expensive)
	 */
	const int num_strokes = BLI_listbase_count(&gpf->strokes);
	int num_valid = 0;

	bool *valid_strokes = MEM_callocN(sizeof(bool) * num_strokes, __func__);

	for (gps = gpf->strokes.first, idx = 0; gps; gps = gps->next, idx++) {
		/* Record whether this stroke can be used
		 * ATTENTION: The logic here is the inverse of what's used everywhere else!
		 */
		if (is_stroke_affected_by_modifier(ob,
		        mmd->layername, mmd->pass_index, 1, gpl, gps,
		        mmd->flag & GP_INSTANCE_INVERT_LAYER, mmd->flag & GP_INSTANCE_INVERT_PASS))
		{
			valid_strokes[idx] = true;
			num_valid++;
		}
	}

	/* Early exit if no strokes can be copied */
	if (num_valid == 0) {
		if (G.debug & G_DEBUG) {
			printf("GP Array Mod - No strokes to be included\n");
		}

		MEM_SAFE_FREE(valid_strokes);
		return;
	}


	/* Generate new instances of all existing strokes,
	 * keeping each instance together so they maintain
	 * the correct ordering relative to each other
	 */
	for (int x = 0; x < mmd->count[0]; x++) {
		for (int y = 0; y < mmd->count[1]; y++) {
			for (int z = 0; z < mmd->count[2]; z++) {
				/* original strokes are at index = 0,0,0 */
				if ((x == 0) && (y == 0) && (z == 0)) {
					continue;
				}

				/* Compute transforms for this instance */
				const int elem_idx[3] = {x, y, z};
				float mat[4][4];

				BKE_gpencil_instance_modifier_instance_tfm(mmd, elem_idx, mat);

				/* apply shift */
				int sh = x;
				if (mmd->lock_axis == GP_LOCKAXIS_Y) {
					sh = y;
				}
				if (mmd->lock_axis == GP_LOCKAXIS_Z) {
					sh = z;
				}
				madd_v3_v3fl(mat[3], mmd->shift, sh);

				/* Duplicate original strokes to create this instance */
				for (gps = gpf->strokes.first, idx = 0; gps; gps = gps->next, idx++) {
					/* check if stroke can be duplicated */
					if (valid_strokes[idx]) {
						/* Duplicate stroke */
						bGPDstroke *gps_dst = MEM_dupallocN(gps);
						gps_dst->points = MEM_dupallocN(gps->points);
						gps_dst->dvert = MEM_dupallocN(gps->dvert);
						BKE_gpencil_stroke_weights_duplicate(gps, gps_dst);

						gps_dst->triangles = MEM_dupallocN(gps->triangles);

						/* Move points */
						for (int i = 0; i < gps->totpoints; i++) {
							bGPDspoint *pt = &gps_dst->points[i];
							mul_m4_v3(mat, &pt->x);
						}

						/* Add new stroke to cache, to be added to the frame once
						 * all duplicates have been made
						 */
						BLI_addtail(&stroke_cache, gps_dst);
					}
				}
			}
		}
	}

	/* merge newly created stroke instances back into the main stroke list */
	BLI_movelisttolist(&gpf->strokes, &stroke_cache);

	/* free temp data */
	MEM_SAFE_FREE(valid_strokes);
}

static void bakeModifier(Main *UNUSED(bmain), Depsgraph *depsgraph,
					GpencilModifierData *md, Object *ob)
{

	bGPdata *gpd = ob->data;

	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			generate_geometry(md, depsgraph, ob, gpl, gpf);
		}
	}
}

/* -------------------------------- */

/* Generic "generateStrokes" callback */
static void generateStrokes(
        GpencilModifierData *md, Depsgraph *depsgraph,
        Object *ob, bGPDlayer *gpl, bGPDframe *gpf)
{
	generate_geometry(md, depsgraph, ob, gpl, gpf);
}

GpencilModifierTypeInfo modifierType_Gpencil_Instance = {
	/* name */              "Instance",
	/* structName */        "InstanceGpencilModifierData",
	/* structSize */        sizeof(InstanceGpencilModifierData),
	/* type */              eGpencilModifierTypeType_Gpencil,
	/* flags */             0,

	/* copyData */          copyData,

	/* deformStroke */      NULL,
	/* generateStrokes */   generateStrokes,
	/* bakeModifier */      bakeModifier,

	/* initData */          initData,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
