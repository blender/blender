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

/** \file blender/modifiers/intern/MOD_gpencilsubdiv.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
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
	bGPDspoint *temp_points;
	int totnewpoints, oldtotpoints;
	int i2;

	if (!is_stroke_affected_by_modifier(ob,
	        mmd->layername, mmd->pass_index, 3, gpl, gps,
	        mmd->flag & GP_SUBDIV_INVERT_LAYER, mmd->flag & GP_SUBDIV_INVERT_PASS))
	{
		return;
	}

	/* loop as many times as levels */
	for (int s = 0; s < mmd->level; s++) {
		totnewpoints = gps->totpoints - 1;
		/* duplicate points in a temp area */
		temp_points = MEM_dupallocN(gps->points);
		oldtotpoints = gps->totpoints;

		/* resize the points arrys */
		gps->totpoints += totnewpoints;
		gps->points = MEM_recallocN(gps->points, sizeof(*gps->points) * gps->totpoints);
		gps->dvert = MEM_recallocN(gps->dvert, sizeof(*gps->dvert) * gps->totpoints);
		gps->flag |= GP_STROKE_RECALC_CACHES;

		/* move points from last to first to new place */
		i2 = gps->totpoints - 1;
		for (int i = oldtotpoints - 1; i > 0; i--) {
			bGPDspoint *pt = &temp_points[i];
			bGPDspoint *pt_final = &gps->points[i2];

			MDeformVert *dvert = &gps->dvert[i];
			MDeformVert *dvert_final = &gps->dvert[i2];

			copy_v3_v3(&pt_final->x, &pt->x);
			pt_final->pressure = pt->pressure;
			pt_final->strength = pt->strength;
			pt_final->time = pt->time;
			pt_final->flag = pt->flag;

			dvert_final->totweight = dvert->totweight;
			dvert_final->dw = dvert->dw;
			i2 -= 2;
		}
		/* interpolate mid points */
		i2 = 1;
		for (int i = 0; i < oldtotpoints - 1; i++) {
			bGPDspoint *pt = &temp_points[i];
			bGPDspoint *next = &temp_points[i + 1];
			bGPDspoint *pt_final = &gps->points[i2];
			MDeformVert *dvert_final = &gps->dvert[i2];

			/* add a half way point */
			interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
			pt_final->pressure = interpf(pt->pressure, next->pressure, 0.5f);
			pt_final->strength = interpf(pt->strength, next->strength, 0.5f);
			CLAMP(pt_final->strength, GPENCIL_STRENGTH_MIN, 1.0f);
			pt_final->time = interpf(pt->time, next->time, 0.5f);

			dvert_final->totweight = 0;
			dvert_final->dw = NULL;
			i2 += 2;
		}

		MEM_SAFE_FREE(temp_points);

		/* move points to smooth stroke (not simple flag )*/
		if ((mmd->flag & GP_SUBDIV_SIMPLE) == 0) {
			/* duplicate points in a temp area with the new subdivide data */
			temp_points = MEM_dupallocN(gps->points);

			/* extreme points are not changed */
			for (int i = 0; i < gps->totpoints - 2; i++) {
				bGPDspoint *pt = &temp_points[i];
				bGPDspoint *next = &temp_points[i + 1];
				bGPDspoint *pt_final = &gps->points[i + 1];

				/* move point */
				interp_v3_v3v3(&pt_final->x, &pt->x, &next->x, 0.5f);
			}
			/* free temp memory */
			MEM_SAFE_FREE(temp_points);
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

GpencilModifierTypeInfo modifierType_Gpencil_Subdiv = {
	/* name */              "Subdivision",
	/* structName */        "SubdivGpencilModifierData",
	/* structSize */        sizeof(SubdivGpencilModifierData),
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
