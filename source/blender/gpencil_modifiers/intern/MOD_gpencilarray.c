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
#include "BKE_modifier.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_layer.h"
#include "BKE_library_query.h"
#include "BKE_collection.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
	ArrayGpencilModifierData *gpmd = (ArrayGpencilModifierData *)md;
	gpmd->count = 2;
	gpmd->offset[0] = 1.0f;
	gpmd->offset[1] = 0.0f;
	gpmd->offset[2] = 0.0f;
	gpmd->shift[0] = 0.0f;
	gpmd->shift[1] = 0.0f;
	gpmd->shift[2] = 0.0f;
	gpmd->scale[0] = 1.0f;
	gpmd->scale[1] = 1.0f;
	gpmd->scale[2] = 1.0f;
	gpmd->rnd_rot = 0.5f;
	gpmd->rnd_size = 0.5f;
	gpmd->object = NULL;

	/* fill random values */
	BLI_array_frand(gpmd->rnd, 20, 1);
	gpmd->rnd[0] = 1;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

/* -------------------------------- */
/* helper function for per-instance positioning */
static void BKE_gpencil_instance_modifier_instance_tfm(
	Object *ob, ArrayGpencilModifierData *mmd, const int elem_idx,
	float r_mat[4][4], float r_offset[4][4])
{
	float offset[3], rot[3], scale[3];
	int ri = mmd->rnd[0];
	float factor;

	offset[0] = mmd->offset[0] * elem_idx;
	offset[1] = mmd->offset[1] * elem_idx;
	offset[2] = mmd->offset[2] * elem_idx;

	/* rotation */
	if (mmd->flag & GP_ARRAY_RANDOM_ROT) {
		factor = mmd->rnd_rot * mmd->rnd[ri];
		mul_v3_v3fl(rot, mmd->rot, factor);
		add_v3_v3(rot, mmd->rot);
	}
	else {
		copy_v3_v3(rot, mmd->rot);
	}

	/* scale */
	if (mmd->flag & GP_ARRAY_RANDOM_SIZE) {
		factor = mmd->rnd_size * mmd->rnd[ri];
		mul_v3_v3fl(scale, mmd->scale, factor);
		add_v3_v3(scale, mmd->scale);
	}
	else {
		copy_v3_v3(scale, mmd->scale);
	}

	/* advance random index */
	mmd->rnd[0]++;
	if (mmd->rnd[0] > 19) {
		mmd->rnd[0] = 1;
	}

	/* calculate matrix */
	loc_eul_size_to_mat4(r_mat, offset, rot, scale);

	copy_m4_m4(r_offset, r_mat);

	/* offset object */
	if (mmd->object) {
		float mat_offset[4][4];
		float obinv[4][4];

		unit_m4(mat_offset);
		add_v3_v3(mat_offset[3], mmd->offset);
		invert_m4_m4(obinv, ob->obmat);

		mul_m4_series(r_offset, mat_offset,
			obinv, mmd->object->obmat);
		copy_m4_m4(mat_offset, r_offset);

		/* clear r_mat locations to avoid double transform */
		zero_v3(r_mat[3]);
	}
}

/* array modifier - generate geometry callback (for viewport/rendering) */
static void generate_geometry(
        GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
        Object *ob, bGPDlayer *gpl, bGPDframe *gpf)
{
	ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;
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
		if (is_stroke_affected_by_modifier(
		            ob,
		            mmd->layername, mmd->pass_index, mmd->layer_pass, 1, gpl, gps,
		            mmd->flag & GP_ARRAY_INVERT_LAYER, mmd->flag & GP_ARRAY_INVERT_PASS,
		            mmd->flag & GP_ARRAY_INVERT_LAYERPASS))
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
	float current_offset[4][4];
	unit_m4(current_offset);

	for (int x = 0; x < mmd->count; x++) {
		/* original strokes are at index = 0 */
		if (x == 0) {
			continue;
		}

		/* Compute transforms for this instance */
		float mat[4][4];
		float mat_offset[4][4];
		BKE_gpencil_instance_modifier_instance_tfm(ob, mmd, x, mat, mat_offset);

		if (mmd->object) {
			/* recalculate cumulative offset here */
			mul_m4_m4m4(current_offset, current_offset, mat_offset);
		}
		else {
			copy_m4_m4(current_offset, mat);
		}
		/* apply shift */
		madd_v3_v3fl(current_offset[3], mmd->shift, x);

		/* Duplicate original strokes to create this instance */
		for (gps = gpf->strokes.first, idx = 0; gps; gps = gps->next, idx++) {
			/* check if stroke can be duplicated */
			if (valid_strokes[idx]) {
				/* Duplicate stroke */
				bGPDstroke *gps_dst = MEM_dupallocN(gps);
				gps_dst->points = MEM_dupallocN(gps->points);
				if (gps->dvert) {
					gps_dst->dvert = MEM_dupallocN(gps->dvert);
					BKE_gpencil_stroke_weights_duplicate(gps, gps_dst);
				}
				gps_dst->triangles = MEM_dupallocN(gps->triangles);

				/* Move points */
				for (int i = 0; i < gps->totpoints; i++) {
					bGPDspoint *pt = &gps_dst->points[i];
					if (mmd->object) {
						/* apply local changes (rot/scale) */
						mul_m4_v3(mat, &pt->x);
					}
					/* global changes */
					mul_m4_v3(current_offset, &pt->x);
				}

				/* if replace material, use new one */
				if ((mmd->mat_rpl > 0) && (mmd->mat_rpl <= ob->totcol)) {
					gps_dst->mat_nr = mmd->mat_rpl - 1;
				}

				/* Add new stroke to cache, to be added to the frame once
				 * all duplicates have been made
				 */
				BLI_addtail(&stroke_cache, gps_dst);
			}
		}
	}

	/* merge newly created stroke instances back into the main stroke list */
	if (mmd->flag & GP_ARRAY_KEEP_ONTOP) {
		BLI_movelisttolist_reverse(&gpf->strokes, &stroke_cache);
	}
	else {
		BLI_movelisttolist(&gpf->strokes, &stroke_cache);
	}

	/* free temp data */
	MEM_SAFE_FREE(valid_strokes);
}

static void bakeModifier(
        Main *UNUSED(bmain), Depsgraph *depsgraph,
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

static void updateDepsgraph(GpencilModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	ArrayGpencilModifierData *lmd = (ArrayGpencilModifierData *)md;
	if (lmd->object != NULL) {
		DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Array Modifier");
		DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Array Modifier");
	}
	DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Array Modifier");
}

static void foreachObjectLink(
	GpencilModifierData *md, Object *ob,
	ObjectWalkFunc walk, void *userData)
{
	ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;

	walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

static int getDuplicationFactor(GpencilModifierData *md)
{
	ArrayGpencilModifierData *mmd = (ArrayGpencilModifierData *)md;
	int t = mmd->count;
	CLAMP_MIN(t, 1);
	return t;
}

GpencilModifierTypeInfo modifierType_Gpencil_Array = {
	/* name */              "Array",
	/* structName */        "ArrayGpencilModifierData",
	/* structSize */        sizeof(ArrayGpencilModifierData),
	/* type */              eGpencilModifierTypeType_Gpencil,
	/* flags */             0,

	/* copyData */          copyData,

	/* deformStroke */      NULL,
	/* generateStrokes */   generateStrokes,
	/* bakeModifier */      bakeModifier,
	/* remapTime */         NULL,

	/* initData */          initData,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
	/* getDuplicationFactor */ getDuplicationFactor,
};
