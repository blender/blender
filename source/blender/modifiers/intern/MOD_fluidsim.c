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

/** \file blender/modifiers/intern/MOD_fluidsim.c
 *  \ingroup modifiers
 */


#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BKE_layer.h"
#include "BKE_modifier.h"

#include "DEG_depsgraph_build.h"

#include "MOD_fluidsim_util.h"
#include "MOD_modifiertypes.h"

#include "MEM_guardedalloc.h"

/* Fluidsim */
static void initData(ModifierData *md)
{
	FluidsimModifierData *fluidmd = (FluidsimModifierData *) md;

	fluidsim_init(fluidmd);
}
static void freeData(ModifierData *md)
{
	FluidsimModifierData *fluidmd = (FluidsimModifierData *) md;

	fluidsim_free(fluidmd);
}

static void copyData(const ModifierData *md, ModifierData *target, const int UNUSED(flag))
{
	const FluidsimModifierData *fluidmd = (const FluidsimModifierData *) md;
	FluidsimModifierData *tfluidmd = (FluidsimModifierData *) target;

	/* Free any FSS that was allocated in initData() */
	if (tfluidmd->fss) {
		MEM_SAFE_FREE(tfluidmd->fss->meshVelocities);
		MEM_freeN(tfluidmd->fss);
	}

	if (fluidmd->fss == NULL) {
		tfluidmd->fss = NULL;
		return;
	}

	tfluidmd->fss = MEM_dupallocN(fluidmd->fss);
	if (tfluidmd->fss->meshVelocities != NULL) {
		tfluidmd->fss->meshVelocities = MEM_dupallocN(tfluidmd->fss->meshVelocities);
	}
}



static Mesh *applyModifier(
        ModifierData *md, const ModifierEvalContext *ctx,
        Mesh *mesh)
{
	FluidsimModifierData *fluidmd = (FluidsimModifierData *) md;
	Mesh *result = NULL;

	/* check for alloc failing */
	if (!fluidmd->fss) {
		initData(md);

		if (!fluidmd->fss) {
			return mesh;
		}
	}

	result = fluidsimModifier_do(fluidmd, ctx, mesh);

	return result ? result : mesh;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	FluidsimModifierData *fluidmd = (FluidsimModifierData *) md;
	if (fluidmd && fluidmd->fss) {
		if (fluidmd->fss->type == OB_FLUIDSIM_DOMAIN) {
			FOREACH_SCENE_OBJECT_BEGIN(ctx->scene, ob1)
			{
				if (ob1 != ctx->object) {
					FluidsimModifierData *fluidmdtmp =
					        (FluidsimModifierData *)modifiers_findByType(ob1, eModifierType_Fluidsim);

					/* Only put dependencies from NON-DOMAIN fluids in here. */
					if (fluidmdtmp && fluidmdtmp->fss && (fluidmdtmp->fss->type != OB_FLUIDSIM_DOMAIN)) {
						DEG_add_object_relation(ctx->node, ob1, DEG_OB_COMP_TRANSFORM, "Fluidsim Object");
					}
				}
			}
			FOREACH_SCENE_OBJECT_END;
		}
	}
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}


ModifierTypeInfo modifierType_Fluidsim = {
	/* name */              "Fluidsim",
	/* structName */        "FluidsimModifierData",
	/* structSize */        sizeof(FluidsimModifierData),
	/* type */              eModifierTypeType_Nonconstructive,

	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_RequiresOriginalData |
	                        eModifierTypeFlag_Single,

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
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,

	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
