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

/** \file blender/modifiers/intern/MOD_smoke.c
 *  \ingroup modifiers
 */


#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"
#include "DNA_object_force.h"

#include "BLI_utildefines.h"


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

static void deformVerts(ModifierData *md, Object *ob,
						DerivedMesh *derivedData,
						float (*vertexCos)[3],
						int UNUSED(numVerts),
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	SmokeModifierData *smd = (SmokeModifierData*) md;
	DerivedMesh *dm = get_cddm(ob, NULL, derivedData, vertexCos);

	smokeModifier_do(smd, md->scene, ob, dm);

	if (dm != derivedData)
		dm->release(dm);
}

static int dependsOnTime(ModifierData *UNUSED(md))
{
	return 1;
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
						struct Scene *scene,
						Object *UNUSED(ob),
						DagNode *obNode)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
		if (smd->domain->fluid_group || smd->domain->coll_group) {
			GroupObject *go = NULL;
			
			if (smd->domain->fluid_group)
				for (go = smd->domain->fluid_group->gobject.first; go; go = go->next) {
					if (go->ob) {
						SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(go->ob, eModifierType_Smoke);
						
						// check for initialized smoke object
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) {
							DagNode *curNode = dag_get_node(forest, go->ob);
							dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Smoke Flow");
						}
					}
				}

			if (smd->domain->coll_group)
				for (go = smd->domain->coll_group->gobject.first; go; go = go->next) {
					if (go->ob) {
						SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(go->ob, eModifierType_Smoke);
						
						// check for initialized smoke object
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll) {
							DagNode *curNode = dag_get_node(forest, go->ob);
							dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Smoke Coll");
						}
					}
				}
		}
		else {
			Base *base = scene->base.first;

			for ( ; base; base = base->next) {
				SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(base->object, eModifierType_Smoke);

				if (smd2 && (((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) || ((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll))) {
					DagNode *curNode = dag_get_node(forest, base->object);
					dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Smoke Flow/Coll");
				}
			}
		}
	}
}

static void foreachIDLink(ModifierData *md, Object *ob,
					   IDWalkFunc walk, void *userData)
{
	SmokeModifierData *smd = (SmokeModifierData*) md;

	if (smd->type==MOD_SMOKE_TYPE_DOMAIN && smd->domain) {
		walk(userData, ob, (ID **)&smd->domain->coll_group);
		walk(userData, ob, (ID **)&smd->domain->fluid_group);
		walk(userData, ob, (ID **)&smd->domain->eff_group);

		if (smd->domain->effector_weights) {
			walk(userData, ob, (ID **)&smd->domain->effector_weights->group);
		}
	}
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
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL
};
