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

#include "stddef.h"

#include "MEM_guardedalloc.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_smoke.h"

#include "depsgraph_private.h"

#include "MOD_util.h"


static void initData(ModifierData *md) 
{
	SmokeModifierData *smd = (SmokeModifierData*) md;
	
	smd->domain = NULL;
	smd->flow = NULL;
	smd->coll = NULL;
	smd->type = 0;
	smd->time = -1;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	SmokeModifierData *smd  = (SmokeModifierData*)md;
	SmokeModifierData *tsmd = (SmokeModifierData*)target;
	
	smokeModifier_copy(smd, tsmd);
}

static void freeData(ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData*) md;
	
	smokeModifier_free (smd);
}

static void deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
	  float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	SmokeModifierData *smd = (SmokeModifierData*) md;
	DerivedMesh *dm = dm= get_cddm(md->scene, ob, NULL, derivedData, vertexCos);

	smokeModifier_do(smd, md->scene, ob, dm, useRenderParams, isFinalCalc);

	if(dm != derivedData)
		dm->release(dm);
}

static int dependsOnTime(ModifierData *md)
{
	return 1;
}

static void updateDepgraph(
					 ModifierData *md, DagForest *forest, struct Scene *scene, Object *ob,
	  DagNode *obNode)
{
	/*SmokeModifierData *smd = (SmokeModifierData *) md;
	if(smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain)
	{
		if(smd->domain->fluid_group)
		{
			GroupObject *go = NULL;
			
			for(go = smd->domain->fluid_group->gobject.first; go; go = go->next) 
			{
				if(go->ob)
				{
					SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(go->ob, eModifierType_Smoke);
					
					// check for initialized smoke object
					if(smd2 && (smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow)
					{
						DagNode *curNode = dag_get_node(forest, go->ob);
						dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Smoke Flow");
					}
				}
			}
		}
	}
	*/
}


ModifierTypeInfo modifierType_Smoke = {
	/* name */              "Smoke",
	/* structName */        "SmokeModifierData",
	/* structSize */        sizeof(SmokeModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_UsesPointCache
							| eModifierTypeFlag_Single,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          freeData,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
