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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_hook.c
 *  \ingroup modifiers
 */


#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_editmesh.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"
#include "BKE_colortools.h"

#include "MEM_guardedalloc.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData *) md;

	hmd->force = 1.0;
	hmd->curfalloff = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
	hmd->falloff_type = eHook_Falloff_Smooth;
	hmd->flag = 0;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
	const HookModifierData *hmd = (const HookModifierData *) md;
	HookModifierData *thmd = (HookModifierData *) target;

	modifier_copyData_generic(md, target, flag);

	thmd->curfalloff = curvemapping_copy(hmd->curfalloff);

	thmd->indexar = MEM_dupallocN(hmd->indexar);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (hmd->name[0]) dataMask |= CD_MASK_MDEFORMVERT;
	if (hmd->indexar) dataMask |= CD_MASK_ORIGINDEX;

	return dataMask;
}

static void freeData(ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData *) md;

	curvemapping_free(hmd->curfalloff);

	MEM_SAFE_FREE(hmd->indexar);
}

static bool isDisabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	HookModifierData *hmd = (HookModifierData *) md;

	return !hmd->object;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	HookModifierData *hmd = (HookModifierData *) md;

	walk(userData, ob, &hmd->object, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	HookModifierData *hmd = (HookModifierData *)md;
	if (hmd->object != NULL) {
		if (hmd->subtarget[0]) {
			DEG_add_bone_relation(ctx->node, hmd->object, hmd->subtarget, DEG_OB_COMP_BONE, "Hook Modifier");
		}
		DEG_add_object_relation(ctx->node, hmd->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
	}
	/* We need own transformation as well. */
	DEG_add_object_relation(ctx->node, ctx->object, DEG_OB_COMP_TRANSFORM, "Hook Modifier");
}

struct HookData_cb {
	float (*vertexCos)[3];

	MDeformVert *dvert;
	int defgrp_index;

	struct CurveMapping *curfalloff;

	char  falloff_type;
	float falloff;
	float falloff_sq;
	float fac_orig;

	unsigned int use_falloff        : 1;
	unsigned int use_uniform        : 1;

	float cent[3];

	float mat_uniform[3][3];
	float mat[4][4];
};

static float hook_falloff(
        const struct HookData_cb *hd,
        const float len_sq)
{
	BLI_assert(hd->falloff_sq);
	if (len_sq > hd->falloff_sq) {
		return 0.0f;
	}
	else if (len_sq > 0.0f) {
		float fac;

		if (hd->falloff_type == eHook_Falloff_Const) {
			fac = 1.0f;
			goto finally;
		}
		else if (hd->falloff_type == eHook_Falloff_InvSquare) {
			/* avoid sqrt below */
			fac = 1.0f - (len_sq / hd->falloff_sq);
			goto finally;
		}

		fac = 1.0f - (sqrtf(len_sq) / hd->falloff);

		/* closely match PROP_SMOOTH and similar */
		switch (hd->falloff_type) {
#if 0
			case eHook_Falloff_None:
				fac = 1.0f;
				break;
#endif
			case eHook_Falloff_Curve:
				fac = curvemapping_evaluateF(hd->curfalloff, 0, fac);
				break;
			case eHook_Falloff_Sharp:
				fac = fac * fac;
				break;
			case eHook_Falloff_Smooth:
				fac = 3.0f * fac * fac - 2.0f * fac * fac * fac;
				break;
			case eHook_Falloff_Root:
				fac = sqrtf(fac);
				break;
			case eHook_Falloff_Linear:
				/* pass */
				break;
#if 0
			case eHook_Falloff_Const:
				fac = 1.0f;
				break;
#endif
			case eHook_Falloff_Sphere:
				fac = sqrtf(2 * fac - fac * fac);
				break;
#if 0
			case eHook_Falloff_InvSquare:
				fac = fac * (2.0f - fac);
				break;
#endif
		}

finally:
		return fac * hd->fac_orig;
	}
	else {
		return hd->fac_orig;
	}
}

static void hook_co_apply(struct HookData_cb *hd, const int j)
{
	float *co = hd->vertexCos[j];
	float fac;

	if (hd->use_falloff) {
		float len_sq;

		if (hd->use_uniform) {
			float co_uniform[3];
			mul_v3_m3v3(co_uniform, hd->mat_uniform, co);
			len_sq = len_squared_v3v3(hd->cent, co_uniform);
		}
		else {
			len_sq = len_squared_v3v3(hd->cent, co);
		}

		fac = hook_falloff(hd, len_sq);
	}
	else {
		fac = hd->fac_orig;
	}

	if (fac) {
		if (hd->dvert) {
			fac *= defvert_find_weight(&hd->dvert[j], hd->defgrp_index);
		}

		if (fac) {
			float co_tmp[3];
			mul_v3_m4v3(co_tmp, hd->mat, co);
			interp_v3_v3v3(co, co, co_tmp, fac);
		}
	}
}

static void deformVerts_do(
        HookModifierData *hmd, Object *ob, Mesh *mesh,
        float (*vertexCos)[3], int numVerts)
{
	bPoseChannel *pchan = BKE_pose_channel_find_name(hmd->object->pose, hmd->subtarget);
	float dmat[4][4];
	int i, *index_pt;
	struct HookData_cb hd;

	if (hmd->curfalloff == NULL) {
		/* should never happen, but bad lib linking could cause it */
		hmd->curfalloff = curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
	}

	if (hmd->curfalloff) {
		curvemapping_initialize(hmd->curfalloff);
	}

	/* Generic data needed for applying per-vertex calculations (initialize all members) */
	hd.vertexCos = vertexCos;
	MOD_get_vgroup(ob, mesh, hmd->name, &hd.dvert, &hd.defgrp_index);

	hd.curfalloff = hmd->curfalloff;

	hd.falloff_type = hmd->falloff_type;
	hd.falloff = (hmd->falloff_type == eHook_Falloff_None) ? 0.0f : hmd->falloff;
	hd.falloff_sq = SQUARE(hd.falloff);
	hd.fac_orig = hmd->force;

	hd.use_falloff = (hd.falloff_sq != 0.0f);
	hd.use_uniform = (hmd->flag & MOD_HOOK_UNIFORM_SPACE) != 0;

	if (hd.use_uniform) {
		copy_m3_m4(hd.mat_uniform, hmd->parentinv);
		mul_v3_m3v3(hd.cent, hd.mat_uniform, hmd->cent);
	}
	else {
		unit_m3(hd.mat_uniform);  /* unused */
		copy_v3_v3(hd.cent, hmd->cent);
	}

	/* get world-space matrix of target, corrected for the space the verts are in */
	if (hmd->subtarget[0] && pchan) {
		/* bone target if there's a matching pose-channel */
		mul_m4_m4m4(dmat, hmd->object->obmat, pchan->pose_mat);
	}
	else {
		/* just object target */
		copy_m4_m4(dmat, hmd->object->obmat);
	}
	invert_m4_m4(ob->imat, ob->obmat);
	mul_m4_series(hd.mat, ob->imat, dmat, hmd->parentinv);
	/* --- done with 'hd' init --- */


	/* Regarding index range checking below.
	 *
	 * This should always be true and I don't generally like
	 * "paranoid" style code like this, but old files can have
	 * indices that are out of range because old blender did
	 * not correct them on exit editmode. - zr
	 */

	if (hmd->force == 0.0f) {
		/* do nothing, avoid annoying checks in the loop */
	}
	else if (hmd->indexar) { /* vertex indices? */
		const int *origindex_ar;

		/* if mesh is present and has original index data, use it */
		if (mesh && (origindex_ar = CustomData_get_layer(&mesh->vdata, CD_ORIGINDEX))) {
			for (i = 0, index_pt = hmd->indexar; i < hmd->totindex; i++, index_pt++) {
				if (*index_pt < numVerts) {
					int j;

					for (j = 0; j < numVerts; j++) {
						if (origindex_ar[j] == *index_pt) {
							hook_co_apply(&hd, j);
						}
					}
				}
			}
		}
		else { /* missing mesh or ORIGINDEX */
			for (i = 0, index_pt = hmd->indexar; i < hmd->totindex; i++, index_pt++) {
				if (*index_pt < numVerts) {
					hook_co_apply(&hd, *index_pt);
				}
			}
		}
	}
	else if (hd.dvert) {  /* vertex group hook */
		for (i = 0; i < numVerts; i++) {
			hook_co_apply(&hd, i);
		}
	}
}

static void deformVerts(
        struct ModifierData *md, const struct ModifierEvalContext *ctx, struct Mesh *mesh,
        float (*vertexCos)[3], int numVerts)
{
	HookModifierData *hmd = (HookModifierData *)md;
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, NULL, mesh, NULL, false, false);

	deformVerts_do(hmd, ctx->object, mesh_src, vertexCos, numVerts);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void deformVertsEM(
        struct ModifierData *md, const struct ModifierEvalContext *ctx,
        struct BMEditMesh *editData,
        struct Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
	HookModifierData *hmd = (HookModifierData *)md;
	Mesh *mesh_src = MOD_get_mesh_eval(ctx->object, editData, mesh, NULL, false, false);

	deformVerts_do(hmd, ctx->object, mesh_src, vertexCos, numVerts);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

ModifierTypeInfo modifierType_Hook = {
	/* name */              "Hook",
	/* structName */        "HookModifierData",
	/* structSize */        sizeof(HookModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_AcceptsLattice |
	                        eModifierTypeFlag_SupportsEditmode,
	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
