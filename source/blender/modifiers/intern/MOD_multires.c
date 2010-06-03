/*
* $Id$
*
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

#include <stddef.h>

#include "BKE_cdderivedmesh.h"
#include "BKE_multires.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_particle.h"

static void initData(ModifierData *md)
{
	MultiresModifierData *mmd = (MultiresModifierData*)md;

	mmd->lvl = 0;
	mmd->sculptlvl = 0;
	mmd->renderlvl = 0;
	mmd->totlvl = 0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	MultiresModifierData *mmd = (MultiresModifierData*) md;
	MultiresModifierData *tmmd = (MultiresModifierData*) target;

	tmmd->lvl = mmd->lvl;
	tmmd->sculptlvl = mmd->sculptlvl;
	tmmd->renderlvl = mmd->renderlvl;
	tmmd->totlvl = mmd->totlvl;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *dm,
						   int useRenderParams, int isFinalCalc)
{
	SculptSession *ss= ob->sculpt;
	int sculpting= (ob->mode & OB_MODE_SCULPT) && ss;
	MultiresModifierData *mmd = (MultiresModifierData*)md;
	DerivedMesh *result;

	result = multires_dm_create_from_derived(mmd, 0, dm, ob, useRenderParams, isFinalCalc);

	if(result == dm)
		return dm;

	if(useRenderParams || !isFinalCalc) {
		DerivedMesh *cddm= CDDM_copy(result);
		result->release(result);
		result= cddm;
	}
	else if(sculpting) {
		/* would be created on the fly too, just nicer this
		   way on first stroke after e.g. switching levels */
		ss->pbvh= result->getPBVH(ob, result);
	}

	return result;
}


ModifierTypeInfo modifierType_Multires = {
	/* name */              "Multires",
	/* structName */        "MultiresModifierData",
	/* structSize */        sizeof(MultiresModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_RequiresOriginalData,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
