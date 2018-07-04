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

/** \file blender/modifiers/intern/MOD_smoke.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_object_force_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_smoke.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_physics.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"

static void initData(ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	smd->domain = NULL;
	smd->flow = NULL;
	smd->coll = NULL;
	smd->type = 0;
	smd->time = -1;
}

static void copyData(const ModifierData *md, ModifierData *target, const int UNUSED(flag))
{
	const SmokeModifierData *smd  = (const SmokeModifierData *)md;
	SmokeModifierData *tsmd = (SmokeModifierData *)target;

	smokeModifier_copy(smd, tsmd);
}

static void freeData(ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	smokeModifier_free(smd);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	SmokeModifierData *smd  = (SmokeModifierData *)md;
	CustomDataMask dataMask = 0;

	if (smd && (smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) {
		if (smd->flow->source == MOD_SMOKE_FLOW_SOURCE_MESH) {
			/* vertex groups */
			if (smd->flow->vgroup_density)
				dataMask |= CD_MASK_MDEFORMVERT;
			/* uv layer */
			if (smd->flow->texture_type == MOD_SMOKE_FLOW_TEXTURE_MAP_UV)
				dataMask |= CD_MASK_MTFACE;
		}
	}
	return dataMask;
}

static DerivedMesh *applyModifier(
        ModifierData *md, const ModifierEvalContext *ctx,
        DerivedMesh *dm)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	if (ctx->flag & MOD_APPLY_ORCO) {
		return dm;
	}

	Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	return smokeModifier_do(smd, ctx->depsgraph, scene, ctx->object, dm);
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static bool is_flow_cb(Object *UNUSED(ob), ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	return (smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow;
}

static bool is_coll_cb(Object *UNUSED(ob), ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	return (smd->type & MOD_SMOKE_TYPE_COLL) && smd->coll;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	SmokeModifierData *smd = (SmokeModifierData *)md;

	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
		DEG_add_collision_relations(ctx->node, ctx->object, smd->domain->fluid_group, eModifierType_Smoke, is_flow_cb, "Smoke Flow");
		DEG_add_collision_relations(ctx->node, ctx->object, smd->domain->coll_group, eModifierType_Smoke, is_coll_cb, "Smoke Coll");
		DEG_add_forcefield_relations(ctx->node, ctx->object, smd->domain->effector_weights, true, PFIELD_SMOKEFLOW, "Smoke Force Field");
	}
}

static void foreachIDLink(
        ModifierData *md, Object *ob,
        IDWalkFunc walk, void *userData)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	if (smd->type == MOD_SMOKE_TYPE_DOMAIN && smd->domain) {
		walk(userData, ob, (ID **)&smd->domain->coll_group, IDWALK_CB_NOP);
		walk(userData, ob, (ID **)&smd->domain->fluid_group, IDWALK_CB_NOP);
		walk(userData, ob, (ID **)&smd->domain->eff_group, IDWALK_CB_NOP);

		if (smd->domain->effector_weights) {
			walk(userData, ob, (ID **)&smd->domain->effector_weights->group, IDWALK_CB_NOP);
		}
	}

	if (smd->type == MOD_SMOKE_TYPE_FLOW && smd->flow) {
		walk(userData, ob, (ID **)&smd->flow->noise_texture, IDWALK_CB_USER);
	}
}

ModifierTypeInfo modifierType_Smoke = {
	/* name */              "Smoke",
	/* structName */        "SmokeModifierData",
	/* structSize */        sizeof(SmokeModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_UsesPointCache |
	                        eModifierTypeFlag_Single,

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
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL
};
