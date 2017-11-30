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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): Based on original depsgraph.c code - Blender Foundation (2005-2013)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_build.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph.
 */

#include "MEM_guardedalloc.h"

// #define DEBUG_TIME

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"

#ifdef DEBUG_TIME
#  include "PIL_time.h"
#  include "PIL_time_utildefines.h"
#endif

extern "C" {
#include "DNA_cachefile_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_force.h"

#include "BKE_main.h"
#include "BKE_collision.h"
#include "BKE_effect.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
} /* extern "C" */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_build.h"

#include "builder/deg_builder.h"
#include "builder/deg_builder_cycle.h"
#include "builder/deg_builder_nodes.h"
#include "builder/deg_builder_relations.h"
#include "builder/deg_builder_transitive.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_types.h"
#include "intern/depsgraph_intern.h"

#include "util/deg_util_foreach.h"

/* ****************** */
/* External Build API */

static DEG::eDepsNode_Type deg_build_scene_component_type(
        eDepsSceneComponentType component)
{
	switch (component) {
		case DEG_SCENE_COMP_PARAMETERS:     return DEG::DEG_NODE_TYPE_PARAMETERS;
		case DEG_SCENE_COMP_ANIMATION:      return DEG::DEG_NODE_TYPE_ANIMATION;
		case DEG_SCENE_COMP_SEQUENCER:      return DEG::DEG_NODE_TYPE_SEQUENCER;
	}
	return DEG::DEG_NODE_TYPE_UNDEFINED;
}

static DEG::eDepsNode_Type deg_build_object_component_type(
        eDepsObjectComponentType component)
{
	switch (component) {
		case DEG_OB_COMP_PARAMETERS:        return DEG::DEG_NODE_TYPE_PARAMETERS;
		case DEG_OB_COMP_PROXY:             return DEG::DEG_NODE_TYPE_PROXY;
		case DEG_OB_COMP_ANIMATION:         return DEG::DEG_NODE_TYPE_ANIMATION;
		case DEG_OB_COMP_TRANSFORM:         return DEG::DEG_NODE_TYPE_TRANSFORM;
		case DEG_OB_COMP_GEOMETRY:          return DEG::DEG_NODE_TYPE_GEOMETRY;
		case DEG_OB_COMP_EVAL_POSE:         return DEG::DEG_NODE_TYPE_EVAL_POSE;
		case DEG_OB_COMP_BONE:              return DEG::DEG_NODE_TYPE_BONE;
		case DEG_OB_COMP_EVAL_PARTICLES:    return DEG::DEG_NODE_TYPE_EVAL_PARTICLES;
		case DEG_OB_COMP_SHADING:           return DEG::DEG_NODE_TYPE_SHADING;
		case DEG_OB_COMP_CACHE:             return DEG::DEG_NODE_TYPE_CACHE;
	}
	return DEG::DEG_NODE_TYPE_UNDEFINED;
}

static DEG::DepsNodeHandle *get_handle(DepsNodeHandle *handle)
{
	return reinterpret_cast<DEG::DepsNodeHandle *>(handle);
}

void DEG_add_scene_relation(DepsNodeHandle *handle,
                            Scene *scene,
                            eDepsSceneComponentType component,
                            const char *description)
{
	DEG::eDepsNode_Type type = deg_build_scene_component_type(component);
	DEG::ComponentKey comp_key(&scene->id, type);
	DEG::DepsNodeHandle *deg_handle = get_handle(handle);
	deg_handle->builder->add_node_handle_relation(comp_key,
	                                              deg_handle,
	                                              description);
}

void DEG_add_object_relation(DepsNodeHandle *handle,
                             Object *object,
                             eDepsObjectComponentType component,
                             const char *description)
{
	DEG::eDepsNode_Type type = deg_build_object_component_type(component);
	DEG::ComponentKey comp_key(&object->id, type);
	DEG::DepsNodeHandle *deg_handle = get_handle(handle);
	deg_handle->builder->add_node_handle_relation(comp_key,
	                                              deg_handle,
	                                              description);
}

void DEG_add_object_cache_relation(DepsNodeHandle *handle,
                                   CacheFile *cache_file,
                                   eDepsObjectComponentType component,
                                   const char *description)
{
	DEG::eDepsNode_Type type = deg_build_object_component_type(component);
	DEG::ComponentKey comp_key(&cache_file->id, type);
	DEG::DepsNodeHandle *deg_handle = get_handle(handle);
	deg_handle->builder->add_node_handle_relation(comp_key,
	                                              deg_handle,
	                                              description);
}

void DEG_add_bone_relation(DepsNodeHandle *handle,
                           Object *object,
                           const char *bone_name,
                           eDepsObjectComponentType component,
                           const char *description)
{
	DEG::eDepsNode_Type type = deg_build_object_component_type(component);
	DEG::ComponentKey comp_key(&object->id, type, bone_name);
	DEG::DepsNodeHandle *deg_handle = get_handle(handle);
	/* XXX: "Geometry Eval" might not always be true, but this only gets called
	 * from modifier building now.
	 */
	deg_handle->builder->add_node_handle_relation(comp_key,
	                                              deg_handle,
	                                              description);
}

struct Depsgraph *DEG_get_graph_from_handle(struct DepsNodeHandle *handle)
{
	DEG::DepsNodeHandle *deg_handle = get_handle(handle);
	DEG::DepsgraphRelationBuilder *relation_builder = deg_handle->builder;
	return reinterpret_cast<Depsgraph *>(relation_builder->getGraph());
}

void DEG_add_special_eval_flag(Depsgraph *graph, ID *id, short flag)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	if (graph == NULL) {
		BLI_assert(!"Graph should always be valid");
		return;
	}
	DEG::IDDepsNode *id_node = deg_graph->find_id_node(id);
	if (id_node == NULL) {
		BLI_assert(!"ID should always be valid");
		return;
	}
	id_node->eval_flags |= flag;
}

/* ******************** */
/* Graph Building API's */

/* Build depsgraph for the given scene layer, and dump results in given
 * graph container.
 */
void DEG_graph_build_from_view_layer(Depsgraph *graph,
                                      Main *bmain,
                                      Scene *scene,
                                      ViewLayer *view_layer)
{
#ifdef DEBUG_TIME
	TIMEIT_START(DEG_graph_build_from_view_layer);
#endif

	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	BLI_assert(BLI_findindex(&scene->view_layers, view_layer) != -1);

	/* TODO(sergey): This is a bit tricky, but ensures that all the data
	 * is evaluated properly when depsgraph is becoming "visible".
	 *
	 * This now could happen for both visible scene is changed and extra
	 * dependency graph was created for render engine.
	 */
	const bool need_on_visible_update = (deg_graph->scene == NULL);

	/* 1) Generate all the nodes in the graph first */
	DEG::DepsgraphNodeBuilder node_builder(bmain, deg_graph);
	node_builder.begin_build();
	node_builder.build_view_layer(scene,
	                               view_layer,
	                               DEG::DEG_ID_LINKED_DIRECTLY);
	node_builder.end_build();

	/* 2) Hook up relationships between operations - to determine evaluation
	 *    order.
	 */
	DEG::DepsgraphRelationBuilder relation_builder(bmain, deg_graph);
	relation_builder.begin_build();
	relation_builder.build_view_layer(scene, view_layer);
	if (DEG_depsgraph_use_copy_on_write()) {
		relation_builder.build_copy_on_write_relations();
	}

	/* Detect and solve cycles. */
	DEG::deg_graph_detect_cycles(deg_graph);

	/* 3) Simplify the graph by removing redundant relations (to optimize
	 *    traversal later). */
	/* TODO: it would be useful to have an option to disable this in cases where
	 *       it is causing trouble.
	 */
	if (G.debug_value == 799) {
		DEG::deg_graph_transitive_reduction(deg_graph);
	}

	/* 4) Flush visibility layer and re-schedule nodes for update. */
	DEG::deg_graph_build_finalize(bmain, deg_graph);

#if 0
	if (!DEG_debug_consistency_check(deg_graph)) {
		printf("Consistency validation failed, ABORTING!\n");
		abort();
	}
#endif

#ifdef DEBUG_TIME
	TIMEIT_END(DEG_graph_build_from_view_layer);
#endif

	/* Relations are up to date. */
	deg_graph->need_update = false;

	if (need_on_visible_update) {
		DEG_graph_on_visible_update(bmain, graph);
	}
}

/* Tag graph relations for update. */
void DEG_graph_tag_relations_update(Depsgraph *graph)
{
	DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(graph);
	deg_graph->need_update = true;
}

/* Create or update relations in the specified graph. */
void DEG_graph_relations_update(Depsgraph *graph,
                                Main *bmain,
                                Scene *scene,
                                ViewLayer *view_layer)
{
	DEG::Depsgraph *deg_graph = (DEG::Depsgraph *)graph;
	if (!deg_graph->need_update) {
		/* Graph is up to date, nothing to do. */
		return;
	}
	DEG_graph_build_from_view_layer(graph, bmain, scene, view_layer);
}

/* Tag all relations for update. */
void DEG_relations_tag_update(Main *bmain)
{
	DEG_DEBUG_PRINTF("%s: Tagging relations for update.\n", __func__);
	LINKLIST_FOREACH(Scene *, scene, &bmain->scene) {
		LINKLIST_FOREACH(ViewLayer *, view_layer, &scene->view_layers) {
			Depsgraph *depsgraph =
			        (Depsgraph *)BKE_scene_get_depsgraph(scene,
			                                             view_layer,
			                                             false);
			if (depsgraph != NULL) {
				DEG_graph_tag_relations_update(depsgraph);
			}
		}
	}
}

void DEG_add_collision_relations(DepsNodeHandle *handle,
                                 Scene *scene,
                                 Object *object,
                                 Group *group,
                                 unsigned int modifier_type,
                                 DEG_CollobjFilterFunction fn,
                                 bool dupli,
                                 const char *name)
{
	unsigned int numcollobj;
	Object **collobjs = get_collisionobjects_ext(scene, object, group, &numcollobj, modifier_type, dupli);

	for (unsigned int i = 0; i < numcollobj; i++) {
		Object *ob1 = collobjs[i];

		if (!fn || fn(ob1, modifiers_findByType(ob1, (ModifierType)modifier_type))) {
			DEG_add_object_relation(handle, ob1, DEG_OB_COMP_TRANSFORM, name);
			DEG_add_object_relation(handle, ob1, DEG_OB_COMP_GEOMETRY, name);
		}
	}

	if (collobjs)
		MEM_freeN(collobjs);
}

void DEG_add_forcefield_relations(DepsNodeHandle *handle,
                                  Scene *scene,
                                  Object *object,
                                  EffectorWeights *effector_weights,
                                  bool add_absorption,
                                  int skip_forcefield,
                                  const char *name)
{
	ListBase *effectors = pdInitEffectors(NULL, scene, object, NULL, effector_weights, false);

	if (effectors) {
		for (EffectorCache *eff = (EffectorCache*)effectors->first; eff; eff = eff->next) {
			if (eff->ob != object && eff->pd->forcefield != skip_forcefield) {
				DEG_add_object_relation(handle, eff->ob, DEG_OB_COMP_TRANSFORM, name);

				if (eff->psys) {
					DEG_add_object_relation(handle, eff->ob, DEG_OB_COMP_EVAL_PARTICLES, name);

					/* TODO: remove this when/if EVAL_PARTICLES is sufficient
					 * for up to date particles.
					 */
					DEG_add_object_relation(handle, eff->ob, DEG_OB_COMP_GEOMETRY, name);
				}

				if (eff->pd->forcefield == PFIELD_SMOKEFLOW && eff->pd->f_source) {
					DEG_add_object_relation(handle,
					                        eff->pd->f_source,
					                        DEG_OB_COMP_TRANSFORM,
					                        "Smoke Force Domain");
					DEG_add_object_relation(handle,
					                        eff->pd->f_source,
					                        DEG_OB_COMP_GEOMETRY,
					                        "Smoke Force Domain");
				}

				if (add_absorption && (eff->pd->flag & PFIELD_VISIBILITY)) {
					DEG_add_collision_relations(handle,
					                            scene,
					                            object,
					                            NULL,
					                            eModifierType_Collision,
					                            NULL,
					                            true,
					                            "Force Absorption");
				}
			}
		}
	}

	pdEndEffectors(&effectors);
}
