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

#include "BLI_math.h"

#include "DNA_key_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_key.h"
#include "BKE_particle.h"

#include "MOD_modifiertypes.h"

#include "MEM_guardedalloc.h"

static void deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
	  float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	KeyBlock *kb= ob_get_keyblock(ob);
	float (*deformedVerts)[3];

	if(kb && kb->totelem == numVerts) {
		deformedVerts= (float(*)[3])do_ob_key(md->scene, ob);
		if(deformedVerts) {
			memcpy(vertexCos, deformedVerts, sizeof(float)*3*numVerts);
			MEM_freeN(deformedVerts);
		}
	}
}

static void deformVertsEM(
					   ModifierData *md, Object *ob, struct EditMesh *editData,
	DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	Key *key= ob_get_key(ob);

	if(key && key->type == KEY_RELATIVE)
		deformVerts(md, ob, derivedData, vertexCos, numVerts, 0, 0);
}

static void deformMatricesEM(
						  ModifierData *md, Object *ob, struct EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3],
						 float (*defMats)[3][3], int numVerts)
{
	Key *key= ob_get_key(ob);
	KeyBlock *kb= ob_get_keyblock(ob);
	float scale[3][3];
	int a;

	if(kb && kb->totelem==numVerts && kb!=key->refkey) {
		scale_m3_fl(scale, kb->curval);

		for(a=0; a<numVerts; a++)
			copy_m3_m3(defMats[a], scale);
	}
}


ModifierTypeInfo modifierType_ShapeKey = {
	/* name */              "ShapeKey",
	/* structName */        "ShapeKeyModifierData",
	/* structSize */        sizeof(ShapeKeyModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsEditmode,

	/* copyData */          0,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  deformMatricesEM,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          0,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
