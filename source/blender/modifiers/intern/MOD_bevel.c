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

/** \file blender/modifiers/intern/MOD_bevel.c
 *  \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "BKE_bmesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"

#include "MOD_util.h"


static void initData(ModifierData *md)
{
	BevelModifierData *bmd = (BevelModifierData*) md;

	bmd->value = 0.1f;
	bmd->res = 1;
	bmd->flags = 0;
	bmd->val_flags = 0;
	bmd->lim_flags = 0;
	bmd->e_flags = 0;
	bmd->bevel_angle = 30;
	bmd->defgrp_name[0] = '\0';
}

static void copyData(ModifierData *md, ModifierData *target)
{
	BevelModifierData *bmd = (BevelModifierData*) md;
	BevelModifierData *tbmd = (BevelModifierData*) target;

	tbmd->value = bmd->value;
	tbmd->res = bmd->res;
	tbmd->flags = bmd->flags;
	tbmd->val_flags = bmd->val_flags;
	tbmd->lim_flags = bmd->lim_flags;
	tbmd->e_flags = bmd->e_flags;
	tbmd->bevel_angle = bmd->bevel_angle;
	BLI_strncpy(tbmd->defgrp_name, bmd->defgrp_name, sizeof(tbmd->defgrp_name));
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	BevelModifierData *bmd = (BevelModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(bmd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *UNUSED(ob),
						DerivedMesh *derivedData,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	DerivedMesh *result;
	BME_Mesh *bm;

	/*bDeformGroup *def;*/
	int /*i,*/ options, defgrp_index = -1;
	BevelModifierData *bmd = (BevelModifierData*) md;

	options = bmd->flags|bmd->val_flags|bmd->lim_flags|bmd->e_flags;

	/*if ((options & BME_BEVEL_VWEIGHT) && bmd->defgrp_name[0]) {
		defgrp_index = defgroup_name_index(ob, bmd->defgrp_name);
		if (defgrp_index < 0) {
			options &= ~BME_BEVEL_VWEIGHT;
		}
	}*/

	bm = BME_derivedmesh_to_bmesh(derivedData);
	BME_bevel(bm,bmd->value,bmd->res,options,defgrp_index,bmd->bevel_angle,NULL);
	result = BME_bmesh_to_derivedmesh(bm,derivedData);
	BME_free_mesh(bm);

	CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
						EditMesh *UNUSED(editData),
						DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_Bevel = {
	/* name */              "Bevel",
	/* structName */        "BevelModifierData",
	/* structSize */        sizeof(BevelModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
