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

/** \file blender/modifiers/intern/MOD_softbody.c
 *  \ingroup modifiers
 */

#include <stdio.h>

#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_softbody.h"

#include "MOD_modifiertypes.h"

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *UNUSED(derivedData),
                        float (*vertexCos)[3],
                        int numVerts,
                        int UNUSED(useRenderParams),
                        int UNUSED(isFinalCalc))
{
	sbObjectStep(md->scene, ob, (float)md->scene->r.cfra, vertexCos, numVerts);
}

static int dependsOnTime(ModifierData *UNUSED(md))
{
	return 1;
}


ModifierTypeInfo modifierType_Softbody = {
	/* name */              "Softbody",
	/* structName */        "SoftbodyModifierData",
	/* structSize */        sizeof(SoftbodyModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_RequiresOriginalData |
	                        eModifierTypeFlag_Single,

	/* copyData */          NULL,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          NULL,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
