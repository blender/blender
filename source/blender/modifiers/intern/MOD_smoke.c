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
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_smoke.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

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

static void update_depsgraph_flow_coll_object(DagForest *forest,
                                              DagNode *obNode,
                                              Object *object2)
{
	SmokeModifierData *smd;
	if ((object2->id.flag & LIB_DOIT) == 0) {
		return;
	}
	object2->id.flag &= ~LIB_DOIT;
	smd = (SmokeModifierData *)modifiers_findByType(object2, eModifierType_Smoke);
	if (smd && (((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) ||
	            ((smd->type & MOD_SMOKE_TYPE_COLL) && smd->coll)))
	{
		DagNode *curNode = dag_get_node(forest, object2);
		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Smoke Flow/Coll");
	}
	if ((object2->transflag & OB_DUPLIGROUP) && object2->dup_group) {
		GroupObject *go;
		for (go = object2->dup_group->gobject.first;
		     go != NULL;
		     go = go->next)
		{
			if (go->ob == NULL) {
				continue;
			}
			update_depsgraph_flow_coll_object(forest, obNode, go->ob);
		}
	}
}

static void update_depsgraph_field_source_object(DagForest *forest,
                                                 DagNode *obNode,
                                                 Object *object,
                                                 Object *object2)
{
	if ((object2->id.flag & LIB_DOIT) == 0) {
		return;
	}
	object2->id.flag &= ~LIB_DOIT;
	if (object2->pd && object2->pd->forcefield == PFIELD_SMOKEFLOW && object2->pd->f_source == object) {
		DagNode *node2 = dag_get_node(forest, object2);
		dag_add_relation(forest, obNode, node2, DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Field Source Object");
	}
	if ((object2->transflag & OB_DUPLIGROUP) && object2->dup_group) {
		GroupObject *go;
		for (go = object2->dup_group->gobject.first;
		     go != NULL;
		     go = go->next)
		{
			if (go->ob == NULL) {
				continue;
			}
			update_depsgraph_field_source_object(forest, obNode, object, go->ob);
		}
	}
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *bmain,
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
			BKE_main_id_tag_listbase(&bmain->object, true);
			base = scene->base.first;
			for (; base; base = base->next) {
				update_depsgraph_flow_coll_object(forest, obNode, base->object);
			}
		}
		/* add relation to all "smoke flow" force fields */
		base = scene->base.first;
		BKE_main_id_tag_listbase(&bmain->object, true);
		for (; base; base = base->next) {
			update_depsgraph_field_source_object(forest, obNode, ob, base->object);
		}
	}
}

static void update_depsgraph_flow_coll_object_new(struct DepsNodeHandle *node,
                                                  Object *object2)
{
	SmokeModifierData *smd;
	if ((object2->id.flag & LIB_DOIT) == 0) {
		return;
	}
	object2->id.flag &= ~LIB_DOIT;
	smd = (SmokeModifierData *)modifiers_findByType(object2, eModifierType_Smoke);
	if (smd && (((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow) ||
	            ((smd->type & MOD_SMOKE_TYPE_COLL) && smd->coll)))
	{
		DEG_add_object_relation(node, object2, DEG_OB_COMP_TRANSFORM, "Smoke Flow/Coll");
		DEG_add_object_relation(node, object2, DEG_OB_COMP_GEOMETRY, "Smoke Flow/Coll");
	}
	if ((object2->transflag & OB_DUPLIGROUP) && object2->dup_group) {
		GroupObject *go;
		for (go = object2->dup_group->gobject.first;
		     go != NULL;
		     go = go->next)
		{
			if (go->ob == NULL) {
				continue;
			}
			update_depsgraph_flow_coll_object_new(node, go->ob);
		}
	}
}

static void update_depsgraph_field_source_object_new(struct DepsNodeHandle *node,
                                                     Object *object,
                                                     Object *object2)
{
	if ((object2->id.flag & LIB_DOIT) == 0) {
		return;
	}
	object2->id.flag &= ~LIB_DOIT;
	if (object2->pd && object2->pd->forcefield == PFIELD_SMOKEFLOW && object2->pd->f_source == object) {
		DEG_add_object_relation(node, object2, DEG_OB_COMP_TRANSFORM, "Field Source Object");
		DEG_add_object_relation(node, object2, DEG_OB_COMP_GEOMETRY, "Field Source Object");
	}
	if ((object2->transflag & OB_DUPLIGROUP) && object2->dup_group) {
		GroupObject *go;
		for (go = object2->dup_group->gobject.first;
		     go != NULL;
		     go = go->next)
		{
			if (go->ob == NULL) {
				continue;
			}
			update_depsgraph_field_source_object_new(node, object, go->ob);
		}
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *bmain,
                            struct Scene *scene,
                            Object *ob,
                            struct DepsNodeHandle *node)
{
	SmokeModifierData *smd = (SmokeModifierData *)md;
	Base *base;
	if (smd && (smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain) {
		if (smd->domain->fluid_group || smd->domain->coll_group) {
			GroupObject *go = NULL;
			if (smd->domain->fluid_group != NULL) {
				for (go = smd->domain->fluid_group->gobject.first; go; go = go->next) {
					if (go->ob != NULL) {
						SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(go->ob, eModifierType_Smoke);
						/* Check for initialized smoke object. */
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow) {
							DEG_add_object_relation(node, go->ob, DEG_OB_COMP_TRANSFORM, "Smoke Flow");
						}
					}
				}
			}
			if (smd->domain->coll_group != NULL) {
				for (go = smd->domain->coll_group->gobject.first; go; go = go->next) {
					if (go->ob != NULL) {
						SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(go->ob, eModifierType_Smoke);
						/* Check for initialized smoke object. */
						if (smd2 && (smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll) {
							DEG_add_object_relation(node, go->ob, DEG_OB_COMP_TRANSFORM, "Smoke Coll");
						}
					}
				}
			}
		}
		else {
			BKE_main_id_tag_listbase(&bmain->object, true);
			base = scene->base.first;
			for (; base; base = base->next) {
				update_depsgraph_flow_coll_object_new(node, base->object);
			}
		}
		/* add relation to all "smoke flow" force fields */
		base = scene->base.first;
		BKE_main_id_tag_listbase(&bmain->object, true);
		for (; base; base = base->next) {
			update_depsgraph_field_source_object_new(node, ob, base->object);
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
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL
};
