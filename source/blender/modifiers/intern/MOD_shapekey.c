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

/** \file blender/modifiers/intern/MOD_shapekey.c
 *  \ingroup modifiers
 */

#include "BLI_math.h"

#include "DNA_key_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_key.h"
#include "BKE_particle.h"

#include "MOD_modifiertypes.h"

static void deformVerts(ModifierData *UNUSED(md), Object *ob,
                        DerivedMesh *UNUSED(derivedData),
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	Key *key = BKE_key_from_object(ob);

	if (key && key->block.first) {
		int deformedVerts_tot;
		BKE_key_evaluate_object_ex(
		            ob, &deformedVerts_tot,
		            (float *)vertexCos, sizeof(*vertexCos) * numVerts);

	}
}

static void deformMatrices(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                           float (*vertexCos)[3], float (*defMats)[3][3], int numVerts)
{
	Key *key = BKE_key_from_object(ob);
	KeyBlock *kb = BKE_keyblock_from_object(ob);
	float scale[3][3];

	(void)vertexCos; /* unused */

	if (kb && kb->totelem == numVerts && kb != key->refkey) {
		int a;

		if (ob->shapeflag & OB_SHAPE_LOCK) scale_m3_fl(scale, 1);
		else scale_m3_fl(scale, kb->curval);

		for (a = 0; a < numVerts; a++)
			copy_m3_m3(defMats[a], scale);
	}

	deformVerts(md, ob, derivedData, vertexCos, numVerts, 0);
}

static void deformVertsEM(ModifierData *md, Object *ob,
                          struct BMEditMesh *UNUSED(editData),
                          DerivedMesh *derivedData,
                          float (*vertexCos)[3],
                          int numVerts)
{
	Key *key = BKE_key_from_object(ob);

	if (key && key->type == KEY_RELATIVE)
		deformVerts(md, ob, derivedData, vertexCos, numVerts, 0);
}

static void deformMatricesEM(ModifierData *UNUSED(md), Object *ob,
                             struct BMEditMesh *UNUSED(editData),
                             DerivedMesh *UNUSED(derivedData),
                             float (*vertexCos)[3],
                             float (*defMats)[3][3],
                             int numVerts)
{
	Key *key = BKE_key_from_object(ob);
	KeyBlock *kb = BKE_keyblock_from_object(ob);
	float scale[3][3];

	(void)vertexCos; /* unused */

	if (kb && kb->totelem == numVerts && kb != key->refkey) {
		int a;
		scale_m3_fl(scale, kb->curval);

		for (a = 0; a < numVerts; a++)
			copy_m3_m3(defMats[a], scale);
	}
}


ModifierTypeInfo modifierType_ShapeKey = {
	/* name */              "ShapeKey",
	/* structName */        "ShapeKeyModifierData",
	/* structSize */        sizeof(ShapeKeyModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          NULL,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    deformMatrices,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  deformMatricesEM,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          NULL,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
