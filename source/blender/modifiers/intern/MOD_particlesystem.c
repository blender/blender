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

/** \file blender/modifiers/intern/MOD_particlesystem.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"


#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"

#include "MOD_util.h"


static void initData(ModifierData *md) 
{
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
	psmd->psys = NULL;
	psmd->dm_final = NULL;
	psmd->dm_deformed = NULL;
	psmd->totdmvert = psmd->totdmedge = psmd->totdmface = 0;
}
static void freeData(ModifierData *md)
{
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;

	if (psmd->dm_final) {
		psmd->dm_final->needsFree = true;
		psmd->dm_final->release(psmd->dm_final);
		psmd->dm_final = NULL;
		if (psmd->dm_deformed) {
			psmd->dm_deformed->needsFree = true;
			psmd->dm_deformed->release(psmd->dm_deformed);
			psmd->dm_deformed = NULL;
		}
	}
}
static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	ParticleSystemModifierData *psmd = (ParticleSystemModifierData *) md;
#endif
	ParticleSystemModifierData *tpsmd = (ParticleSystemModifierData *) target;

	modifier_copyData_generic(md, target);

	tpsmd->dm_final = NULL;
	tpsmd->dm_deformed = NULL;
	tpsmd->totdmvert = tpsmd->totdmedge = tpsmd->totdmface = 0;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *UNUSED(md))
{
	return 0;
}

/* saves the current emitter state for a particle system and calculates particles */
static void deformVerts(ModifierData *UNUSED(md), Object *UNUSED(ob),
                        DerivedMesh *UNUSED(derivedData),
                        float (*vertexCos)[3],
                        int UNUSED(numVerts),
                        ModifierApplyFlag UNUSED(flag))
{
	UNUSED_VARS(vertexCos);
	return;
}

/* disabled particles in editmode for now, until support for proper derivedmesh
 * updates is coded */
#if 0
static void deformVertsEM(
        ModifierData *md, Object *ob, EditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if (!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	deformVerts(md, ob, dm, vertexCos, numVerts);

	if (!derivedData) dm->release(dm);
}
#endif


ModifierTypeInfo modifierType_ParticleSystem = {
	/* name */              "ParticleSystem",
	/* structName */        "ParticleSystemModifierData",
	/* structSize */        sizeof(ParticleSystemModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_UsesPointCache /* |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode */,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     NULL,
	/* deformMatrices */    NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
