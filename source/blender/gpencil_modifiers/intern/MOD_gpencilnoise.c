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

/** \file blender/gpencil_modifiers/intern/MOD_gpencilnoise.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"

#include "PIL_time.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

static void initData(GpencilModifierData *md)
{
	NoiseGpencilModifierData *gpmd = (NoiseGpencilModifierData *)md;
	gpmd->pass_index = 0;
	gpmd->flag |= GP_NOISE_MOD_LOCATION;
	gpmd->flag |= GP_NOISE_FULL_STROKE;
	gpmd->flag |= GP_NOISE_USE_RANDOM;
	gpmd->factor = 0.5f;
	gpmd->layername[0] = '\0';
	gpmd->vgname[0] = '\0';
	gpmd->step = 1;
	gpmd->scene_frame = -999999;
	gpmd->gp_frame = -999999;

	gpmd->vrand1 = 1.0;
	gpmd->vrand2 = 1.0;
}

static void freeData(GpencilModifierData *md)
{
	NoiseGpencilModifierData *mmd = (NoiseGpencilModifierData *)md;

	if (mmd->rng != NULL) {
		BLI_rng_free(mmd->rng);
	}
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

static bool dependsOnTime(GpencilModifierData *md)
{
	NoiseGpencilModifierData *mmd = (NoiseGpencilModifierData *)md;
	return (mmd->flag & GP_NOISE_USE_RANDOM) != 0;
}

/* aply noise effect based on stroke direction */
static void deformStroke(
        GpencilModifierData *md, Depsgraph *depsgraph,
        Object *ob, bGPDlayer *gpl, bGPDstroke *gps)
{
	NoiseGpencilModifierData *mmd = (NoiseGpencilModifierData *)md;
	bGPDspoint *pt0, *pt1;
	MDeformVert *dvert;
	float shift, vran, vdir;
	float normal[3];
	float vec1[3], vec2[3];
#if 0
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
#endif
	int sc_frame = 0;
	int sc_diff = 0;
	int vindex = defgroup_name_index(ob, mmd->vgname);
	float weight = 1.0f;

	/* Random generator, only init once. */
	if (mmd->rng == NULL) {
		uint rng_seed = (uint)(PIL_check_seconds_timer_i() & UINT_MAX);
		rng_seed ^= GET_UINT_FROM_POINTER(mmd);
		mmd->rng = BLI_rng_new(rng_seed);
	}

	if (!is_stroke_affected_by_modifier(ob,
	        mmd->layername, mmd->pass_index, 3, gpl, gps,
	        mmd->flag & GP_NOISE_INVERT_LAYER, mmd->flag & GP_NOISE_INVERT_PASS))
	{
		return;
	}

	sc_frame = (int)DEG_get_ctime(depsgraph);

	zero_v3(vec2);

	/* calculate stroke normal*/
	BKE_gpencil_stroke_normal(gps, normal);

	/* move points */
	for (int i = 0; i < gps->totpoints; i++) {
		if (((i == 0) || (i == gps->totpoints - 1)) && ((mmd->flag & GP_NOISE_MOVE_EXTREME) == 0)) {
			continue;
		}

		/* last point is special */
		if (i == gps->totpoints) {
			dvert = &gps->dvert[i - 2];
			pt0 = &gps->points[i - 2];
			pt1 = &gps->points[i - 1];
		}
		else {
			dvert = &gps->dvert[i - 1];
			pt0 = &gps->points[i - 1];
			pt1 = &gps->points[i];

		}

		/* verify vertex group */
		weight = get_modifier_point_weight(dvert, (int)((mmd->flag & GP_NOISE_INVERT_VGROUP) != 0), vindex);
		if (weight < 0) {
			continue;
		}

		/* initial vector (p0 -> p1) */
		sub_v3_v3v3(vec1, &pt1->x, &pt0->x);
		vran = len_v3(vec1);
		/* vector orthogonal to normal */
		cross_v3_v3v3(vec2, vec1, normal);
		normalize_v3(vec2);
		/* use random noise */
		if (mmd->flag & GP_NOISE_USE_RANDOM) {
			sc_diff = abs(mmd->scene_frame - sc_frame);
			/* only recalc if the gp frame change or the number of scene frames is bigger than step */
			if ((!gpl->actframe) || (mmd->gp_frame != gpl->actframe->framenum) ||
			    (sc_diff >= mmd->step))
			{
				vran = mmd->vrand1 = BLI_rng_get_float(mmd->rng);
				vdir = mmd->vrand2 = BLI_rng_get_float(mmd->rng);
				mmd->gp_frame = gpl->actframe->framenum;
				mmd->scene_frame = sc_frame;
			}
			else {
				vran = mmd->vrand1;
				if (mmd->flag & GP_NOISE_FULL_STROKE) {
					vdir = mmd->vrand2;
				}
				else {
					int f = (mmd->vrand2 * 10.0f) + i;
					vdir = f % 2;
				}
			}
		}
		else {
			vran = 1.0f;
			if (mmd->flag & GP_NOISE_FULL_STROKE) {
				vdir = gps->totpoints % 2;
			}
			else {
				vdir = i % 2;
			}
			mmd->gp_frame = -999999;
		}

		/* apply randomness to location of the point */
		if (mmd->flag & GP_NOISE_MOD_LOCATION) {
			/* factor is too sensitive, so need divide */
			shift = ((vran * mmd->factor) / 1000.0f) * weight;
			if (vdir > 0.5f) {
				mul_v3_fl(vec2, shift);
			}
			else {
				mul_v3_fl(vec2, shift * -1.0f);
			}
			add_v3_v3(&pt1->x, vec2);
		}

		/* apply randomness to thickness */
		if (mmd->flag & GP_NOISE_MOD_THICKNESS) {
			if (vdir > 0.5f) {
				pt1->pressure -= pt1->pressure * vran * mmd->factor;
			}
			else {
				pt1->pressure += pt1->pressure * vran * mmd->factor;
			}
			CLAMP_MIN(pt1->pressure, GPENCIL_STRENGTH_MIN);
		}

		/* apply randomness to color strength */
		if (mmd->flag & GP_NOISE_MOD_STRENGTH) {
			if (vdir > 0.5f) {
				pt1->strength -= pt1->strength * vran * mmd->factor;
			}
			else {
				pt1->strength += pt1->strength * vran * mmd->factor;
			}
			CLAMP_MIN(pt1->strength, GPENCIL_STRENGTH_MIN);
		}
		/* apply randomness to uv rotation */
		if (mmd->flag & GP_NOISE_MOD_UV) {
			if (vdir > 0.5f) {
				pt1->uv_rot -= pt1->uv_rot * vran * mmd->factor;
			}
			else {
				pt1->uv_rot += pt1->uv_rot * vran * mmd->factor;
			}
			CLAMP(pt1->uv_rot, -M_PI_2, M_PI_2);
		}
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

GpencilModifierTypeInfo modifierType_Gpencil_Noise = {
	/* name */              "Noise",
	/* structName */        "NoiseGpencilModifierData",
	/* structSize */        sizeof(NoiseGpencilModifierData),
	/* type */              eGpencilModifierTypeType_Gpencil,
	/* flags */             eGpencilModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,

	/* deformStroke */      deformStroke,
	/* generateStrokes */   NULL,
	/* bakeModifier */    bakeModifier,

	/* initData */          initData,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
