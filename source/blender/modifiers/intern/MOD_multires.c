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

/** \file blender/modifiers/intern/MOD_multires.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_modifier.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subdiv_mesh.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md)
{
	MultiresModifierData *mmd = (MultiresModifierData *)md;

	mmd->lvl = 0;
	mmd->sculptlvl = 0;
	mmd->renderlvl = 0;
	mmd->totlvl = 0;
	mmd->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_CORNERS;
	mmd->quality = 3;
}

#ifndef WITH_OPENSUBDIV_MODIFIER

static DerivedMesh *applyModifier_DM(
        ModifierData *md, const ModifierEvalContext *ctx,
        DerivedMesh *dm)
{
	MultiresModifierData *mmd = (MultiresModifierData *)md;
	struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	DerivedMesh *result;
	Mesh *me = (Mesh *)ctx->object->data;
	const bool useRenderParams = (ctx->flag & MOD_APPLY_RENDER) != 0;
	const bool ignore_simplify = (ctx->flag & MOD_APPLY_IGNORE_SIMPLIFY) != 0;
	MultiresFlags flags = 0;
	const bool has_mask = CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK);

	if (mmd->totlvl) {
		if (!CustomData_get_layer(&me->ldata, CD_MDISPS)) {
			/* multires always needs a displacement layer */
			CustomData_add_layer(&me->ldata, CD_MDISPS, CD_CALLOC, NULL, me->totloop);
		}
	}

	if (has_mask)
		flags |= MULTIRES_ALLOC_PAINT_MASK;

	if (useRenderParams)
		flags |= MULTIRES_USE_RENDER_PARAMS;

	if (ignore_simplify)
		flags |= MULTIRES_IGNORE_SIMPLIFY;

	result = multires_make_derived_from_derived(dm, mmd, scene, ctx->object, flags);

	if (result == dm)
		return dm;

	if (useRenderParams || !(ctx->flag & MOD_APPLY_USECACHE)) {
		DerivedMesh *cddm;

		cddm = CDDM_copy(result);

		/* copy hidden/masks to vertices */
		if (!useRenderParams) {
			struct MDisps *mdisps;
			struct GridPaintMask *grid_paint_mask;

			mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
			grid_paint_mask = CustomData_get_layer(&me->ldata, CD_GRID_PAINT_MASK);

			if (mdisps) {
				subsurf_copy_grid_hidden(result, me->mpoly,
				                         cddm->getVertArray(cddm),
				                         mdisps);

				BKE_mesh_flush_hidden_from_verts_ex(cddm->getVertArray(cddm),
				                                    cddm->getLoopArray(cddm),
				                                    cddm->getEdgeArray(cddm),
				                                    cddm->getNumEdges(cddm),
				                                    cddm->getPolyArray(cddm),
				                                    cddm->getNumPolys(cddm));
			}
			if (grid_paint_mask) {
				float *paint_mask = CustomData_add_layer(&cddm->vertData,
				                                         CD_PAINT_MASK,
				                                         CD_CALLOC, NULL,
				                                         cddm->getNumVerts(cddm));

				subsurf_copy_grid_paint_mask(result, me->mpoly,
				                             paint_mask, grid_paint_mask);
			}
		}

		result->release(result);
		result = cddm;
	}

	return result;
}

applyModifier_DM_wrapper(applyModifier, applyModifier_DM)

#endif

#ifdef WITH_OPENSUBDIV_MODIFIER

/* Subdivide into fully qualified mesh. */

static Mesh *multires_as_mesh(MultiresModifierData *mmd,
                              const ModifierEvalContext *ctx,
                              Mesh *mesh,
                              Subdiv *subdiv)
{
	Mesh *result = mesh;
	const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
	const bool ignore_simplify = (ctx->flag & MOD_APPLY_IGNORE_SIMPLIFY);
	const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	Object *object = ctx->object;
	SubdivToMeshSettings mesh_settings;
	BKE_multires_subdiv_mesh_settings_init(
        &mesh_settings, scene, object, mmd, use_render_params, ignore_simplify);
	if (mesh_settings.resolution < 3) {
		return result;
	}
	BKE_subdiv_displacement_attach_from_multires(subdiv, mesh, mmd);
	result = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh);
	return result;
}

/* Subdivide into CCG. */

static void multires_ccg_settings_init(SubdivToCCGSettings *settings,
                                       const MultiresModifierData *mmd,
                                       const ModifierEvalContext *ctx,
                                       Mesh *mesh)
{
	const bool has_mask =
	        CustomData_has_layer(&mesh->ldata, CD_GRID_PAINT_MASK);
	const bool use_render_params = (ctx->flag & MOD_APPLY_RENDER);
	const bool ignore_simplify = (ctx->flag & MOD_APPLY_IGNORE_SIMPLIFY);
	const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	Object *object = ctx->object;
	const int level = multires_get_level(
	        scene, object, mmd, use_render_params, ignore_simplify);
	settings->resolution = (1 << level) + 1;
	settings->need_normal = true;
	settings->need_mask = has_mask;
}

static Mesh *multires_as_ccg(MultiresModifierData *mmd,
                             const ModifierEvalContext *ctx,
                             Mesh *mesh,
                             Subdiv *subdiv)
{
	Mesh *result = mesh;
	SubdivToCCGSettings ccg_settings;
	multires_ccg_settings_init(&ccg_settings, mmd, ctx, mesh);
	if (ccg_settings.resolution < 3) {
		return result;
	}
	BKE_subdiv_displacement_attach_from_multires(subdiv, mesh, mmd);
	result = BKE_subdiv_to_ccg_mesh(subdiv, &ccg_settings, mesh);
	return result;
}

static Mesh *applyModifier_subdiv(ModifierData *md,
                                  const ModifierEvalContext *ctx,
                                  Mesh *mesh)
{
	Mesh *result = mesh;
	MultiresModifierData *mmd = (MultiresModifierData *)md;
	SubdivSettings subdiv_settings;
	BKE_multires_subdiv_settings_init(&subdiv_settings, mmd);
	if (subdiv_settings.level == 0) {
		return result;
	}
	/* TODO(sergey): Try to re-use subdiv when possible. */
	Subdiv *subdiv = BKE_subdiv_new_from_mesh(&subdiv_settings, mesh);
	if (subdiv == NULL) {
		/* Happens on bad topology, ut also on empty input mesh. */
		return result;
	}
	/* TODO(sergey): Some of production machines are using OpenSubdiv already.
	 * so better not enable semi-finished multires sculpting for now. Will give
	 * a wrong impression that things do work, even though crucial areas are
	 * still missing in implementation.
	 */
	const bool for_orco = (ctx->flag & MOD_APPLY_ORCO) != 0;
	if ((ctx->object->mode & OB_MODE_SCULPT) &&
	    G.debug_value == 128 &&
	    !for_orco)
	{
		/* NOTE: CCG takes ownership over Subdiv. */
		result = multires_as_ccg(mmd, ctx, mesh, subdiv);
		// BKE_subdiv_stats_print(&subdiv->stats);
	}
	else {
		result = multires_as_mesh(mmd, ctx, mesh, subdiv);
		/* TODO(sergey): Cache subdiv somehow. */
		// BKE_subdiv_stats_print(&subdiv->stats);
		BKE_subdiv_free(subdiv);
	}
	return result;
}
#endif

ModifierTypeInfo modifierType_Multires = {
	/* name */              "Multires",
	/* structName */        "MultiresModifierData",
	/* structSize */        sizeof(MultiresModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_RequiresOriginalData,

	/* copyData */          modifier_copyData_generic,

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
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
