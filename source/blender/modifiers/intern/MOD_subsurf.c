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

/** \file blender/modifiers/intern/MOD_subsurf.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_scene.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_mesh.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

#include "intern/CCGSubSurf.h"

static void initData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;

	smd->levels = 1;
	smd->renderLevels = 2;
	smd->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_CORNERS;
	smd->quality = 3;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
#if 0
	const SubsurfModifierData *smd = (const SubsurfModifierData *) md;
#endif
	SubsurfModifierData *tsmd = (SubsurfModifierData *) target;

	modifier_copyData_generic(md, target, flag);

	tsmd->emCache = tsmd->mCache = NULL;
}

static void freeData(ModifierData *md)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;

	if (smd->mCache) {
		ccgSubSurf_free(smd->mCache);
		smd->mCache = NULL;
	}
	if (smd->emCache) {
		ccgSubSurf_free(smd->emCache);
		smd->emCache = NULL;
	}
}

static bool isDisabled(const Scene *scene, ModifierData *md, bool useRenderParams)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	int levels = (useRenderParams) ? smd->renderLevels : smd->levels;

	return get_render_subsurf_level(&scene->r, levels, useRenderParams != 0) == 0;
}

#ifndef WITH_OPENSUBDIV_MODIFIER

static DerivedMesh *applyModifier_DM(
        ModifierData *md, const ModifierEvalContext *ctx,
        DerivedMesh *derivedData)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	SubsurfFlags subsurf_flags = 0;
	DerivedMesh *result;
	const bool useRenderParams = (ctx->flag & MOD_APPLY_RENDER) != 0;
	const bool isFinalCalc = (ctx->flag & MOD_APPLY_USECACHE) != 0;

	bool do_cddm_convert = useRenderParams || !isFinalCalc;

	if (useRenderParams)
		subsurf_flags |= SUBSURF_USE_RENDER_PARAMS;
	if (isFinalCalc)
		subsurf_flags |= SUBSURF_IS_FINAL_CALC;
	if (ctx->object->mode & OB_MODE_EDIT)
		subsurf_flags |= SUBSURF_IN_EDIT_MODE;

	result = subsurf_make_derived_from_derived(derivedData, smd, scene, NULL, subsurf_flags);
	result->cd_flag = derivedData->cd_flag;

	{
		DerivedMesh *cddm = CDDM_copy(result);
		result->release(result);
		result = cddm;
	}

	(void) do_cddm_convert;

	return result;
}

applyModifier_DM_wrapper(applyModifier, applyModifier_DM)

#endif

static DerivedMesh *applyModifierEM_DM(
        ModifierData *md, const ModifierEvalContext *ctx,
        struct BMEditMesh *UNUSED(editData),
        DerivedMesh *derivedData)
{
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	DerivedMesh *result;
	/* 'orco' using editmode flags would cause cache to be used twice in editbmesh_calc_modifiers */
	SubsurfFlags ss_flags = (ctx->flag & MOD_APPLY_ORCO) ? 0 : (SUBSURF_FOR_EDIT_MODE | SUBSURF_IN_EDIT_MODE);

	result = subsurf_make_derived_from_derived(derivedData, smd, scene, NULL, ss_flags);
	return result;
}

static Mesh *applyModifierEM(
        struct ModifierData *md, const struct ModifierEvalContext *ctx,
        struct BMEditMesh *editData,
        struct Mesh *mesh)
{
	DerivedMesh *dm = CDDM_from_mesh_ex(mesh, CD_REFERENCE, CD_MASK_EVERYTHING);
	DerivedMesh *ndm = applyModifierEM_DM(md, ctx, editData, dm);
	if (ndm != dm) {
		dm->release(dm);
	}
	DM_to_mesh(ndm, mesh, ctx->object, CD_MASK_EVERYTHING, true);
	return mesh;
}

#ifdef WITH_OPENSUBDIV_MODIFIER
static int subdiv_levels_for_modifier_get(const SubsurfModifierData *smd,
                                          const ModifierEvalContext *ctx)
{
	Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
	const int requested_levels = (use_render_params) ? smd->renderLevels
	                                                 : smd->levels;
	return get_render_subsurf_level(&scene->r,
	                                requested_levels,
	                                use_render_params);
}

static void subdiv_settings_init(SubdivSettings *settings,
                                 const SubsurfModifierData *smd)
{
	settings->is_simple = (smd->subdivType == SUBSURF_TYPE_SIMPLE);
	settings->is_adaptive = !settings->is_simple;
	settings->level = smd->quality;
	settings->fvar_linear_interpolation =
	        BKE_subdiv_fvar_interpolation_from_uv_smooth(smd->uv_smooth);
}

/* Subdivide into fully qualified mesh. */

static void subdiv_mesh_settings_init(SubdivToMeshSettings *settings,
                                      const SubsurfModifierData *smd,
                                      const ModifierEvalContext *ctx)
{
	const int level = subdiv_levels_for_modifier_get(smd, ctx);
	settings->resolution = (1 << level) + 1;
}

static Mesh *subdiv_as_mesh(SubsurfModifierData *smd,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            Subdiv *subdiv)
{
	Mesh *result = mesh;
	SubdivToMeshSettings mesh_settings;
	subdiv_mesh_settings_init(&mesh_settings, smd, ctx);
	if (mesh_settings.resolution < 3) {
		return result;
	}
	result = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh);
	return result;
}

/* Subdivide into CCG. */

static void subdiv_ccg_settings_init(SubdivToCCGSettings *settings,
                                     const SubsurfModifierData *smd,
                                     const ModifierEvalContext *ctx)
{
	const int level = subdiv_levels_for_modifier_get(smd, ctx);
	settings->resolution = (1 << level) + 1;
	settings->need_normal = true;
	settings->need_mask = false;
}

static Mesh *subdiv_as_ccg(SubsurfModifierData *smd,
                            const ModifierEvalContext *ctx,
                            Mesh *mesh,
                            Subdiv *subdiv)
{
	Mesh *result = mesh;
	SubdivToCCGSettings ccg_settings;
	subdiv_ccg_settings_init(&ccg_settings, smd, ctx);
	if (ccg_settings.resolution < 3) {
		return result;
	}
	result = BKE_subdiv_to_ccg_mesh(subdiv, &ccg_settings, mesh);
	return result;
}

/* Modifier itself. */

static Mesh *applyModifier_subdiv(ModifierData *md,
                                  const ModifierEvalContext *ctx,
                                  Mesh *mesh)
{
	Mesh *result = mesh;
	SubsurfModifierData *smd = (SubsurfModifierData *) md;
	SubdivSettings subdiv_settings;
	subdiv_settings_init(&subdiv_settings, smd);
	if (subdiv_settings.level == 0) {
		return result;
	}
	/* TODO(sergey): Try to re-use subdiv when possible. */
	Subdiv *subdiv = BKE_subdiv_new_from_mesh(&subdiv_settings, mesh);
	if (subdiv == NULL) {
		/* Happens on bad topology, ut also on empty input mesh. */
		return result;
	}
	/* TODO(sergey): Decide whether we ever want to use CCG for subsurf,
	 * maybe when it is a last modifier in the stack?
	 */
	if (true) {
		result = subdiv_as_mesh(smd, ctx, mesh, subdiv);
	}
	else {
		result = subdiv_as_ccg(smd, ctx, mesh, subdiv);
	}
	/* TODO(sergey): Cache subdiv somehow. */
	// BKE_subdiv_stats_print(&subdiv->stats);
	BKE_subdiv_free(subdiv);
	return result;
}
#endif

ModifierTypeInfo modifierType_Subsurf = {
	/* name */              "Subdivision",
	/* structName */        "SubsurfModifierData",
	/* structSize */        sizeof(SubsurfModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode |
	                        eModifierTypeFlag_AcceptsCVs,

	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
#ifdef WITH_OPENSUBDIV_MODIFIER
	/* applyModifier */     applyModifier_subdiv,
#else
	/* applyModifier */     applyModifier,
#endif
	/* applyModifierEM */   applyModifierEM,

	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
