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

static void initData(ModifierData *md) 
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	
	smd->domain = NULL;
	smd->flow = NULL;
	smd->coll = NULL;
	smd->type = 0;
	smd->time = -1;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	SmokeModifierData *smd  = (SmokeModifierData *)md;
	SmokeModifierData *tsmd = (SmokeModifierData *)target;
	
	smokeModifier_copy(smd, tsmd);
}

static void freeData(ModifierData *md)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	
	smokeModifier_free(smd);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	SmokeModifierData *smd  = (SmokeModifierData *)md;
	CustomDataMask dataMask = 0;

	if (smd && (smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) {
		if (smd->flow->source == MOD_SMOKE_FLOW_SOURCE_MESH) {
			/* vertex groups */
			if (smd->flow->vgroup_density)
				dataMask |= CD_MASK_MDEFORMVERT;
			/* uv layer */
			if (smd->flow->texture_type == MOD_SMOKE_FLOW_TEXTURE_MAP_UV)
				dataMask |= CD_MASK_MTFACE;
		}
	}
	return dataMask;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, 
                                  DerivedMesh *dm,
                                  ModifierApplyFlag flag)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	bool for_render = (flag & MOD_APPLY_RENDER) != 0;

	if (flag & MOD_APPLY_ORCO)
		return dm;

	return smokeModifier_do(smd, md->scene, ob, dm, for_render);
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *scene, struct Object *ob,
                           DagNode *obNode)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;
	Base *base;

	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
		if (smd->domain->fluid_group || smd->domain->coll_group) {
			GroupObject *go = NULL;
			
			if (smd->domain->fluid_group)
				for (go = smd->domain->fluid_group->gobject.first; go; go = go->next) {
					if (go->ob) {
						SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(go->ob, eModifierType_Smoke);
						
						/* check for initialized smoke object */
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) {
							DagNode *curNode = dag_get_node(forest, go->ob);
							dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Smoke Flow");
						}
					}
				}

			if (smd->domain->coll_group)
				for (go = smd->domain->coll_group->gobject.first; go; go = go->next) {
					if (go->ob) {
						SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(go->ob, eModifierType_Smoke);
						
						/* check for initialized smoke object */
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll) {
							DagNode *curNode = dag_get_node(forest, go->ob);
							dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Smoke Coll");
						}
					}
				}
		}
		else {
			base = scene->base.first;
			for (; base; base = base->next) {
				SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(base->object, eModifierType_Smoke);

				if (smd2 && (((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) || ((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll))) {
					DagNode *curNode = dag_get_node(forest, base->object);
					dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Smoke Flow/Coll");
				}
			}
		}
		/* add relation to all "smoke flow" force fields */
		base = scene->base.first;
		for (; base; base = base->next) {
			if (base->object->pd && base->object->pd->forcefield == PFIELD_SMOKEFLOW && base->object->pd->f_source == ob) {
				DagNode *node2 = dag_get_node(forest, base->object);
				dag_add_relation(forest, obNode, node2, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Field Source Object");
			}
		}
	}
}

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	SmokeModifierData *smd = (SmokeModifierData *) md;

	if (smd->type == MOD_SMOKE_TYPE_DOMAIN && smd->domain) {
		walk(userData, ob, (ID **)&smd->domain->coll_group);
		walk(userData, ob, (ID **)&smd->domain->fluid_group);
		walk(userData, ob, (ID **)&smd->domain->eff_group);

		if (smd->domain->effector_weights) {
			walk(userData, ob, (ID **)&smd->domain->effector_weights->group);
		}
	}

	if (smd->type == MOD_SMOKE_TYPE_FLOW && smd->flow) {
		walk(userData, ob, (ID **)&smd->flow->noise_texture);
	}
}

ModifierTypeInfo modifierType_Smoke = {
	/* name */              "Smoke",
	/* structName */        "SmokeModifierData",
	/* structSize */        sizeof(SmokeModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_UsesPointCache |
	                        eModifierTypeFlag_Single,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL
};
