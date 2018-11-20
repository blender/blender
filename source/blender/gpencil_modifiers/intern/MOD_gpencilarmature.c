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
 * The Original Code is Copyright (C) 2018, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/gpencil_modifiers/intern/MOD_gpencilarmature.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_armature_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "BLI_math.h"

#include "BLI_listbase.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_lattice.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_modifier.h"
#include "BKE_library_query.h"
#include "BKE_scene.h"
#include "BKE_main.h"

#include "MEM_guardedalloc.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

static void initData(GpencilModifierData *md)
{
	ArmatureGpencilModifierData *gpmd = (ArmatureGpencilModifierData *)md;
	gpmd->object = NULL;
	gpmd->deformflag = ARM_DEF_VGROUP;
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	BKE_gpencil_modifier_copyData_generic(md, target);
}

static void gpencil_deform_verts(
	ArmatureGpencilModifierData *mmd, Object *target,
	bGPDstroke *gps)
{
	bGPDspoint *pt = gps->points;
	float *all_vert_coords = MEM_callocN(sizeof(float) * 3 * gps->totpoints, __func__);
	int i;

	BKE_gpencil_dvert_ensure(gps);

	/* prepare array of points */
	for (i = 0; i < gps->totpoints; i++, pt++) {
		float *pt_coords = &all_vert_coords[3 * i];
		float co[3];
		copy_v3_v3(co, &pt->x);
		copy_v3_v3(pt_coords, co);
	}

	/* deform verts */
	armature_deform_verts(
	        mmd->object, target, NULL,
	        (float(*)[3])all_vert_coords,
	        NULL, gps->totpoints,
	        mmd->deformflag,
	        (float(*)[3])mmd->prevCos,
	        mmd->vgname, gps);

	/* Apply deformed coordinates */
	pt = gps->points;
	for (i = 0; i < gps->totpoints; i++, pt++) {
		float *pt_coords = &all_vert_coords[3 * i];
		copy_v3_v3(&pt->x, pt_coords);
	}

	MEM_SAFE_FREE(all_vert_coords);

}

/* deform stroke */
static void deformStroke(
        GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
        Object *ob, bGPDlayer *UNUSED(gpl), bGPDstroke *gps)
{
	ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;
	if (!mmd->object) {
		return;
	}

	gpencil_deform_verts(mmd, ob, gps);
}

static void bakeModifier(
        Main *bmain, Depsgraph *depsgraph,
        GpencilModifierData *md, Object *ob)
{
	ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	bGPdata *gpd = ob->data;
	int oldframe = (int)DEG_get_ctime(depsgraph);

	if (mmd->object == NULL)
		return;

	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			/* apply armature effects on this frame
			 * NOTE: this assumes that we don't want armature animation on non-keyframed frames
			 */
			CFRA = gpf->framenum;
			BKE_scene_graph_update_for_newframe(depsgraph, bmain);

			/* compute armature effects on this frame */
			for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
				deformStroke(md, depsgraph, ob, gpl, gps);
			}
		}
	}

	/* return frame state and DB to original state */
	CFRA = oldframe;
	BKE_scene_graph_update_for_newframe(depsgraph, bmain);
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
	ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;

	return !mmd->object;
}

static void updateDepsgraph(GpencilModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	ArmatureGpencilModifierData *lmd = (ArmatureGpencilModifierData *)md;
	if (lmd->object != NULL) {
		DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_EVAL_POSE, "Armature Modifier");
		DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Armature Modifier");
	}
	DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Armature Modifier");
}

static void foreachObjectLink(
        GpencilModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	ArmatureGpencilModifierData *mmd = (ArmatureGpencilModifierData *)md;

	walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

GpencilModifierTypeInfo modifierType_Gpencil_Armature = {
	/* name */              "Armature",
	/* structName */        "ArmatureGpencilModifierData",
	/* structSize */        sizeof(ArmatureGpencilModifierData),
	/* type */              eGpencilModifierTypeType_Gpencil,
	/* flags */             0,

	/* copyData */          copyData,

	/* deformStroke */      deformStroke,
	/* generateStrokes */   NULL,
	/* bakeModifier */      bakeModifier,
	/* remapTime */         NULL,
	/* initData */          initData,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
	/* getDuplicationFactor */ NULL,
};
