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
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md) 
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *) md;
	
	pmd->canvas = NULL;
	pmd->brush = NULL;
	pmd->type = MOD_DYNAMICPAINT_TYPE_CANVAS;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	DynamicPaintModifierData *pmd  = (DynamicPaintModifierData *)md;
	DynamicPaintModifierData *tpmd = (DynamicPaintModifierData *)target;
	
	dynamicPaint_Modifier_copy(pmd, tpmd);

	if (tpmd->canvas) {
		for (DynamicPaintSurface *surface = tpmd->canvas->surfaces.first; surface; surface = surface->next) {
			id_us_plus((ID *)surface->init_texture);
		}
	}
	if (tpmd->brush) {
		id_us_plus((ID *)tpmd->brush->mat);
	}
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
				dataMask |= CD_MASK_MLOOPUV | CD_MASK_MTEXPOLY;
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

	if (pmd->brush) {
		if (pmd->brush->flags & MOD_DPAINT_USE_MATERIAL) {
			dataMask |= CD_MASK_MLOOPUV | CD_MASK_MTEXPOLY;
		}
	}
	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, 
                                  DerivedMesh *dm,
                                  ModifierApplyFlag flag)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *) md;

	/* dont apply dynamic paint on orco dm stack */
	if (!(flag & MOD_APPLY_ORCO)) {
		return dynamicPaint_Modifier_do(pmd, md->scene, ob, dm);
	}
	return dm;
}

static bool is_brush_cb(Object *UNUSED(ob), ModifierData *pmd)
{
	return ((DynamicPaintModifierData *)pmd)->brush != NULL;
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *scene,
                           Object *ob,
                           DagNode *obNode)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *) md;

	/* add relation from canvases to all brush objects */
	if (pmd && pmd->canvas) {
#ifdef WITH_LEGACY_DEPSGRAPH
		for (DynamicPaintSurface *surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
			if (surface->effect & MOD_DPAINT_EFFECT_DO_DRIP) {
				dag_add_forcefield_relations(forest, scene, ob, obNode, surface->effector_weights, true, 0, "Dynamic Paint Field");
			}

			/* Actual code uses custom loop over group/scene without layer checks in dynamicPaint_doStep */
			dag_add_collision_relations(forest, scene, ob, obNode, surface->brush_group, -1, eModifierType_DynamicPaint, is_brush_cb, false, "Dynamic Paint Brush");
		}
#else
	(void)forest;
	(void)scene;
	(void)ob;
	(void)obNode;
#endif
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *scene,
                            Object *ob,
                            struct DepsNodeHandle *node)
{
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)md;
	/* Add relation from canvases to all brush objects. */
	if (pmd->canvas != NULL) {
		for (DynamicPaintSurface *surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
			if (surface->effect & MOD_DPAINT_EFFECT_DO_DRIP) {
				DEG_add_forcefield_relations(node, scene, ob, surface->effector_weights, true, 0, "Dynamic Paint Field");
			}

			/* Actual code uses custom loop over group/scene without layer checks in dynamicPaint_doStep */
			DEG_add_collision_relations(node, scene, ob, surface->brush_group, -1, eModifierType_DynamicPaint, is_brush_cb, false, "Dynamic Paint Brush");
		}
	}
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void foreachIDLink(ModifierData *md, Object *ob,
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
	if (pmd->brush) {
		walk(userData, ob, (ID **)&pmd->brush->mat, IDWALK_CB_USER);
	}
}

static void foreachTexLink(ModifierData *UNUSED(md), Object *UNUSED(ob),
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
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
