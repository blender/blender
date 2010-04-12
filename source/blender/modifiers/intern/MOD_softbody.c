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

#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_softbody.h"

#include "MOD_modifiertypes.h"

static void deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
	  float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	sbObjectStep(md->scene, ob, (float)md->scene->r.cfra, vertexCos, numVerts);
}

static int dependsOnTime(ModifierData *md)
{
	return 1;
}


ModifierTypeInfo modifierType_Softbody = {
	/* name */              "Softbody",
	/* structName */        "SoftbodyModifierData",
	/* structSize */        sizeof(SoftbodyModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_RequiresOriginalData
							| eModifierTypeFlag_Single,

	/* copyData */          0,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          0,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
