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

#include "string.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_lattice.h"
#include "BKE_modifier.h"

#include "depsgraph_private.h"

#include "MOD_util.h"


static void copyData(ModifierData *md, ModifierData *target)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;
	LatticeModifierData *tlmd = (LatticeModifierData*) target;

	tlmd->object = lmd->object;
	strncpy(tlmd->name, lmd->name, 32);
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	LatticeModifierData *lmd = (LatticeModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(lmd->name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static int isDisabled(ModifierData *md, int userRenderParams)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	return !lmd->object;
}

static void foreachObjectLink(
						  ModifierData *md, Object *ob,
	   void (*walk)(void *userData, Object *ob, Object **obpoin),
		  void *userData)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	walk(userData, ob, &lmd->object);
}

static void updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *scene,
					   Object *ob, DagNode *obNode)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;

	if(lmd->object) {
		DagNode *latNode = dag_get_node(forest, lmd->object);

		dag_add_relation(forest, latNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Lattice Modifier");
	}
}

static void deformVerts(
					ModifierData *md, Object *ob, DerivedMesh *derivedData,
	 float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	LatticeModifierData *lmd = (LatticeModifierData*) md;


	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */
	
	lattice_deform_verts(lmd->object, ob, derivedData,
				 vertexCos, numVerts, lmd->name);
}

static void deformVertsEM(
					  ModifierData *md, Object *ob, struct EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	deformVerts(md, ob, dm, vertexCos, numVerts, 0, 0);

	if(!derivedData) dm->release(dm);
}


ModifierTypeInfo modifierType_Lattice = {
	/* name */              "Lattice",
	/* structName */        "LatticeModifierData",
	/* structSize */        sizeof(LatticeModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsEditmode,
	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          0,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          0,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
