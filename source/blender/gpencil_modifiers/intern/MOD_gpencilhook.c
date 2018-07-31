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

/** \file blender/modifiers/intern/MOD_gpencilhook.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "BLI_math.h"

#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_colortools.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_modifier.h"
#include "BKE_library_query.h"
#include "BKE_scene.h"
#include "BKE_main.h"
#include "BKE_layer.h"

#include "MEM_guardedalloc.h"

#include "MOD_gpencil_util.h"
#include "MOD_gpencil_modifiertypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

/* temp struct to hold data */
struct GPHookData_cb {
	struct CurveMapping *curfalloff;

	char  falloff_type;
	float falloff;
	float falloff_sq;
	float fac_orig;

	unsigned int use_falloff : 1;
	unsigned int use_uniform : 1;

	float cent[3];

	float mat_uniform[3][3];
	float mat[4][4];
};

static void initData(GpencilModifierData *md)
{
	HookGpencilModifierData *gpmd = (HookGpencilModifierData *)md;
	gpmd->pass_index = 0;
	gpmd->layername[0] = '\0';
	gpmd->vgname[0] = '\0';
	gpmd->object = NULL;
	gpmd->force = 0.5f;
	gpmd->falloff_type = eGPHook_Falloff_Smooth;
	gpmd->curfalloff = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
	if (gpmd->curfalloff) {
		curvemapping_initialize(gpmd->curfalloff);
	}
}

static void copyData(const GpencilModifierData *md, GpencilModifierData *target)
{
	HookGpencilModifierData *gmd = (HookGpencilModifierData *)md;
	HookGpencilModifierData *tgmd = (HookGpencilModifierData *)target;

	if (tgmd->curfalloff != NULL) {
		curvemapping_free(tgmd->curfalloff);
		tgmd->curfalloff = NULL;
	}

	BKE_gpencil_modifier_copyData_generic(md, target);

	tgmd->curfalloff = curvemapping_copy(gmd->curfalloff);
}

/* calculate factor of fallof */
static float gp_hook_falloff(const struct GPHookData_cb *tData,	const float len_sq)
{
	BLI_assert(tData->falloff_sq);
	if (len_sq > tData->falloff_sq) {
		return 0.0f;
	}
	else if (len_sq > 0.0f) {
		float fac;

		if (tData->falloff_type == eGPHook_Falloff_Const) {
			fac = 1.0f;
			goto finally;
		}
		else if (tData->falloff_type == eGPHook_Falloff_InvSquare) {
			/* avoid sqrt below */
			fac = 1.0f - (len_sq / tData->falloff_sq);
			goto finally;
		}

		fac = 1.0f - (sqrtf(len_sq) / tData->falloff);

		switch (tData->falloff_type) {
			case eGPHook_Falloff_Curve:
				fac = curvemapping_evaluateF(tData->curfalloff, 0, fac);
				break;
			case eGPHook_Falloff_Sharp:
				fac = fac * fac;
				break;
			case eGPHook_Falloff_Smooth:
				fac = 3.0f * fac * fac - 2.0f * fac * fac * fac;
				break;
			case eGPHook_Falloff_Root:
				fac = sqrtf(fac);
				break;
			case eGPHook_Falloff_Linear:
				/* pass */
				break;
			case eGPHook_Falloff_Sphere:
				fac = sqrtf(2 * fac - fac * fac);
				break;
			default:
				fac = fac;
				break;
		}

		finally:
		return fac * tData->fac_orig;
	}
	else {
		return tData->fac_orig;
	}
}

/* apply point deformation */
static void gp_hook_co_apply(struct GPHookData_cb *tData, float weight, bGPDspoint *pt)
{
	float fac;

	if (tData->use_falloff) {
		float len_sq;

		if (tData->use_uniform) {
			float co_uniform[3];
			mul_v3_m3v3(co_uniform, tData->mat_uniform, &pt->x);
			len_sq = len_squared_v3v3(tData->cent, co_uniform);
		}
		else {
			len_sq = len_squared_v3v3(tData->cent, &pt->x);
		}

		fac = gp_hook_falloff(tData, len_sq);
	}
	else {
		fac = tData->fac_orig;
	}

	if (fac) {
		float co_tmp[3];
		mul_v3_m4v3(co_tmp, tData->mat, &pt->x);
		interp_v3_v3v3(&pt->x, &pt->x, co_tmp, fac * weight);
	}
}

/* deform stroke */
static void deformStroke(
        GpencilModifierData *md, Depsgraph *UNUSED(depsgraph),
        Object *ob, bGPDlayer *gpl, bGPDstroke *gps)
{
	HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;
	if (!mmd->object) {
		return;
	}

	int vindex = defgroup_name_index(ob, mmd->vgname);
	float weight = 1.0f;

	bPoseChannel *pchan = BKE_pose_channel_find_name(mmd->object->pose, mmd->subtarget);
	float dmat[4][4];
	struct GPHookData_cb tData;

	if (!is_stroke_affected_by_modifier(ob,
	        mmd->layername, mmd->pass_index, 3, gpl, gps,
	        mmd->flag & GP_HOOK_INVERT_LAYER, mmd->flag & GP_HOOK_INVERT_PASS))
	{
		return;
	}

	/* init struct */
	tData.curfalloff = mmd->curfalloff;
	tData.falloff_type = mmd->falloff_type;
	tData.falloff = (mmd->falloff_type == eHook_Falloff_None) ? 0.0f : mmd->falloff;
	tData.falloff_sq = SQUARE(tData.falloff);
	tData.fac_orig = mmd->force;
	tData.use_falloff = (tData.falloff_sq != 0.0f);
	tData.use_uniform = (mmd->flag & GP_HOOK_UNIFORM_SPACE) != 0;

	if (tData.use_uniform) {
		copy_m3_m4(tData.mat_uniform, mmd->parentinv);
		mul_v3_m3v3(tData.cent, tData.mat_uniform, mmd->cent);
	}
	else {
		unit_m3(tData.mat_uniform);
		copy_v3_v3(tData.cent, mmd->cent);
	}

	/* get world-space matrix of target, corrected for the space the verts are in */
	if (mmd->subtarget[0] && pchan) {
		/* bone target if there's a matching pose-channel */
		mul_m4_m4m4(dmat, mmd->object->obmat, pchan->pose_mat);
	}
	else {
		/* just object target */
		copy_m4_m4(dmat, mmd->object->obmat);
	}
	invert_m4_m4(ob->imat, ob->obmat);
	mul_m4_series(tData.mat, ob->imat, dmat, mmd->parentinv);

	/* loop points and apply deform */
	for (int i = 0; i < gps->totpoints; i++) {
		bGPDspoint *pt = &gps->points[i];
		MDeformVert *dvert = &gps->dvert[i];

		/* verify vertex group */
		weight = get_modifier_point_weight(dvert, (int)(!(mmd->flag & GP_HOOK_INVERT_VGROUP) == 0), vindex);
		if (weight < 0) {
			continue;
		}
		gp_hook_co_apply(&tData, weight, pt);
	}
}

/* FIXME: Ideally we be doing this on a copy of the main depsgraph
 * (i.e. one where we don't have to worry about restoring state)
 */
static void bakeModifier(
		Main *bmain, Depsgraph *depsgraph,
        GpencilModifierData *md, Object *ob)
{
	HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;
	Scene *scene = DEG_get_evaluated_scene(depsgraph);
	bGPdata *gpd = ob->data;
	int oldframe = (int)DEG_get_ctime(depsgraph);

	if (mmd->object == NULL)
		return;

	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
			/* apply hook effects on this frame
			 * NOTE: this assumes that we don't want hook animation on non-keyframed frames
			 */
			CFRA = gpf->framenum;
			BKE_scene_graph_update_for_newframe(depsgraph, bmain);

			/* compute hook effects on this frame */
			for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
				deformStroke(md, depsgraph, ob, gpl, gps);
			}
		}
	}

	/* return frame state and DB to original state */
	CFRA = oldframe;
	BKE_scene_graph_update_for_newframe(depsgraph, bmain);
}

static void freeData(GpencilModifierData *md)
{
	HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;

	if (mmd->curfalloff) {
		curvemapping_free(mmd->curfalloff);
	}
}

static bool isDisabled(GpencilModifierData *md, int UNUSED(userRenderParams))
{
	HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;

	return !mmd->object;
}

static void updateDepsgraph(GpencilModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	HookGpencilModifierData *lmd = (HookGpencilModifierData *)md;
	if (lmd->object != NULL) {
		DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_GEOMETRY, "Hook Modifier");
		DEG_add_object_relation(ctx->node, lmd->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
	}
	DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
}

static void foreachObjectLink(
        GpencilModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	HookGpencilModifierData *mmd = (HookGpencilModifierData *)md;

	walk(userData, ob, &mmd->object, IDWALK_CB_NOP);
}

GpencilModifierTypeInfo modifierType_Gpencil_Hook = {
	/* name */              "Hook",
	/* structName */        "HookGpencilModifierData",
	/* structSize */        sizeof(HookGpencilModifierData),
	/* type */              eGpencilModifierTypeType_Gpencil,
	/* flags */             0,

	/* copyData */          copyData,

	/* deformStroke */      deformStroke,
	/* generateStrokes */   NULL,
	/* bakeModifier */    bakeModifier,

	/* initData */          initData,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
