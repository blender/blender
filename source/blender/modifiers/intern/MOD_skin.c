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
* ***** END GPL LICENSE BLOCK *****
*
*/

/** \file blender/modifiers/intern/MOD_skin.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	SkinModifierData *smd = (SkinModifierData*)md;

	smd->threshold = 0;
	smd->subdiv = 1;
	smd->flag = MOD_SKIN_DRAW_NODES;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	SkinModifierData *smd = (SkinModifierData*) md;
	SkinModifierData *tsmd = (SkinModifierData*) target;

	tsmd->threshold = smd->threshold;
	tsmd->subdiv = smd->subdiv;
	tsmd->flag = smd->flag;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *dm,
						   int useRenderParams, int isFinalCalc)
{
	return dm;
}


ModifierTypeInfo modifierType_Skin = {
	/* name */              "Skin",
	/* structName */        "SkinModifierData",
	/* structSize */        sizeof(SkinModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
};
