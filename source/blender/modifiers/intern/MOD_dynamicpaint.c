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
 * Contributor(s): Miika Hämäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_dynamicpaint.c
 *  \ingroup modifiers
 */

#include <stddef.h>

#include "DNA_dynamicpaint_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_dynamicpaint.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *) md;

	pmd->canvas = NULL;
	pmd->brush = NULL;
	pmd->type = MOD_DYNAMICPAINT_TYPE_CANVAS;
}

static void copyData(const ModifierData *md, ModifierData *target, const int UNUSED(flag))
{
	const DynamicPaintModifierData *pmd  = (const DynamicPaintModifierData *)md;
	DynamicPaintModifierData *tpmd = (DynamicPaintModifierData *)target;

	dynamicPaint_Modifier_copy(pmd, tpmd);
}

static void freeData(ModifierData *md)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *) md;
	dynamicPaint_Modifier_free(pmd);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
	CustomDataMask dataMask = 0;

	if (pmd->canvas) {
		DynamicPaintSurface *surface = pmd->canvas->surfaces.first;
		for (; surface; surface = surface->next) {
			/* tface */
			if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ ||
			    surface->init_color_type == MOD_DPAINT_INITIAL_TEXTURE)
			{
				dataMask |= CD_MASK_MLOOPUV;
			}
			/* mcol */
			if (surface->type == MOD_DPAINT_SURFACE_T_PAINT ||
			    surface->init_color_type == MOD_DPAINT_INITIAL_VERTEXCOLOR)
			{
				dataMask |= CD_MASK_MLOOPCOL;
			}
			/* CD_MDEFORMVERT */
			if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
				dataMask |= CD_MASK_MDEFORMVERT;
			}
		}
	}
	return dataMask;
}

static DerivedMesh *applyModifier(
        ModifierData *md, const ModifierEvalContext *ctx,
        DerivedMesh *dm)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *) md;

	/* dont apply dynamic paint on orco dm stack */
	if (!(ctx->flag & MOD_APPLY_ORCO)) {
		Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
		return dynamicPaint_Modifier_do(pmd, ctx->depsgraph, scene, ctx->object, dm);
	}
	return dm;
}

static bool is_brush_cb(Object *UNUSED(ob), ModifierData *pmd)
{
	return ((DynamicPaintModifierData *)pmd)->brush != NULL;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
	/* Add relation from canvases to all brush objects. */
	if (pmd->canvas != NULL) {
		for (DynamicPaintSurface *surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
			if (surface->effect & MOD_DPAINT_EFFECT_DO_DRIP) {
				DEG_add_forcefield_relations(ctx->node, ctx->object, surface->effector_weights, true, 0, "Dynamic Paint Field");
			}

			/* Actual code uses custom loop over group/scene without layer checks in dynamicPaint_doStep */
			DEG_add_collision_relations(ctx->node, ctx->object, surface->brush_group,  eModifierType_DynamicPaint, is_brush_cb, "Dynamic Paint Brush");
		}
	}
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void foreachIDLink(
        ModifierData *md, Object *ob,
        IDWalkFunc walk, void *userData)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *) md;

	if (pmd->canvas) {
		DynamicPaintSurface *surface = pmd->canvas->surfaces.first;

		for (; surface; surface = surface->next) {
			walk(userData, ob, (ID **)&surface->brush_group, IDWALK_CB_NOP);
			walk(userData, ob, (ID **)&surface->init_texture, IDWALK_CB_USER);
			if (surface->effector_weights) {
				walk(userData, ob, (ID **)&surface->effector_weights->group, IDWALK_CB_NOP);
			}
		}
	}
}

static void foreachTexLink(
        ModifierData *UNUSED(md), Object *UNUSED(ob),
        TexWalkFunc UNUSED(walk), void *UNUSED(userData))
{
	//walk(userData, ob, md, ""); /* re-enable when possible */
}

ModifierTypeInfo modifierType_DynamicPaint = {
	/* name */              "Dynamic Paint",
	/* structName */        "DynamicPaintModifierData",
	/* structSize */        sizeof(DynamicPaintModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
/*	                        eModifierTypeFlag_SupportsMapping |*/
	                        eModifierTypeFlag_UsesPointCache |
	                        eModifierTypeFlag_Single |
	                        eModifierTypeFlag_UsesPreview,

	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  applyModifier,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
