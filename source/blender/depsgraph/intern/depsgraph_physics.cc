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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_physics.cc
 *  \ingroup depsgraph
 *
 * Physics utilities for effectors and collision.
 */

#include "MEM_guardedalloc.h"

#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"

extern "C" {
#include "BKE_collision.h"
#include "BKE_effect.h"
} /* extern "C" */

#include "DNA_group_types.h"

#include "DEG_depsgraph_physics.h"

#include "depsgraph.h"
#include "depsgraph_intern.h"

/************************ Public API *************************/

ListBase *DEG_get_effector_relations(const Depsgraph *graph,
                                     Collection *collection)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	if (deg_graph->effector_relations == NULL) {
		return NULL;
	}

	return (ListBase *)BLI_ghash_lookup(deg_graph->effector_relations, collection);
}

ListBase *DEG_get_collision_relations(const Depsgraph *graph,
                                      Collection *collection)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	if (deg_graph->collision_relations == NULL) {
		return NULL;
	}

	return (ListBase*)BLI_ghash_lookup(deg_graph->collision_relations, collection);
}

ListBase *DEG_get_smoke_collision_relations(const Depsgraph *graph,
                                            Collection *collection)
{
	const DEG::Depsgraph *deg_graph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	if (deg_graph->smoke_collision_relations == NULL) {
		return NULL;
	}

	return (ListBase*)BLI_ghash_lookup(deg_graph->smoke_collision_relations, collection);
}

/*********************** Internal API ************************/

namespace DEG
{

ListBase *deg_build_effector_relations(Depsgraph *graph,
                                       Collection *collection)
{
	if (graph->effector_relations == NULL) {
		graph->effector_relations = BLI_ghash_ptr_new("Depsgraph effector relations hash");
	}

	ListBase *relations = reinterpret_cast<ListBase*>(BLI_ghash_lookup(graph->effector_relations, collection));
	if (relations == NULL) {
		::Depsgraph *depsgraph = reinterpret_cast<::Depsgraph*>(graph);
		relations = BKE_effector_relations_create(depsgraph, graph->view_layer, collection);
		BLI_ghash_insert(graph->effector_relations, collection, relations);
	}

	return relations;
}

ListBase *deg_build_collision_relations(Depsgraph *graph,
                                        Collection *collection)
{
	if (graph->collision_relations == NULL) {
		graph->collision_relations = BLI_ghash_ptr_new("Depsgraph collision relations hash");
	}

	ListBase *relations = reinterpret_cast<ListBase*>(BLI_ghash_lookup(graph->collision_relations, collection));
	if (relations == NULL) {
		::Depsgraph *depsgraph = reinterpret_cast<::Depsgraph*>(graph);
		relations = BKE_collision_relations_create(depsgraph, collection, eModifierType_Collision);
		BLI_ghash_insert(graph->collision_relations, collection, relations);
	}

	return relations;
}

ListBase *deg_build_smoke_collision_relations(Depsgraph *graph,
                                              Collection *collection)
{
	if (graph->smoke_collision_relations == NULL) {
		graph->smoke_collision_relations = BLI_ghash_ptr_new("Depsgraph smoke collision relations hash");
	}

	ListBase *relations = reinterpret_cast<ListBase*>(BLI_ghash_lookup(graph->smoke_collision_relations, collection));
	if (relations == NULL) {
		::Depsgraph *depsgraph = reinterpret_cast<::Depsgraph*>(graph);
		relations = BKE_collision_relations_create(depsgraph, collection, eModifierType_Smoke);
		BLI_ghash_insert(graph->smoke_collision_relations, collection, relations);
	}

	return relations;
}

static void free_effector_relations(void *value)
{
	BKE_effector_relations_free(reinterpret_cast<ListBase*>(value));
}

static void free_collision_relations(void *value)
{
	BKE_collision_relations_free(reinterpret_cast<ListBase*>(value));
}

void deg_clear_physics_relations(Depsgraph *graph)
{
	if (graph->collision_relations) {
		BLI_ghash_free(graph->collision_relations, NULL, free_collision_relations);
		graph->collision_relations = NULL;
	}
	if (graph->smoke_collision_relations) {
		BLI_ghash_free(graph->smoke_collision_relations, NULL, free_collision_relations);
		graph->smoke_collision_relations = NULL;
	}
	if (graph->effector_relations) {
		BLI_ghash_free(graph->effector_relations, NULL, free_effector_relations);
		graph->effector_relations = NULL;
	}
}

}
