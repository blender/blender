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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder.cc
 *  \ingroup depsgraph
 */

#include "intern/builder/deg_builder.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_ID.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_stack.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_types.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"

#include "util/deg_util_foreach.h"

#include <cstdio>

namespace DEG {

static bool check_object_needs_evaluation(Object *object)
{
	if (object->recalc & OB_RECALC_ALL) {
		/* Object is tagged for update anyway, no need to re-tag it. */
		return false;
	}
	if (object->type == OB_MESH) {
		return object->derivedFinal == NULL;
	}
	else if (ELEM(object->type,
	              OB_CURVE, OB_SURF, OB_FONT, OB_MBALL, OB_LATTICE))
	{
		return object->curve_cache == NULL;
	}
	return false;
}

void deg_graph_build_flush_layers(Depsgraph *graph)
{
	BLI_Stack *stack = BLI_stack_new(sizeof(OperationDepsNode *),
	                                 "DEG flush layers stack");
	foreach (OperationDepsNode *node, graph->operations) {
		IDDepsNode *id_node = node->owner->owner;
		node->done = 0;
		node->num_links_pending = 0;
		foreach (DepsRelation *rel, node->outlinks) {
			if ((rel->from->type == DEG_NODE_TYPE_OPERATION) &&
			    (rel->flag & DEPSREL_FLAG_CYCLIC) == 0)
			{
				++node->num_links_pending;
			}
		}
		if (node->num_links_pending == 0) {
			BLI_stack_push(stack, &node);
			node->done = 1;
		}
		node->owner->layers = id_node->layers;
		id_node->id->tag |= LIB_TAG_DOIT;
	}
	while (!BLI_stack_is_empty(stack)) {
		OperationDepsNode *node;
		BLI_stack_pop(stack, &node);
		/* Flush layers to parents. */
		foreach (DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEG_NODE_TYPE_OPERATION) {
				OperationDepsNode *from = (OperationDepsNode *)rel->from;
				from->owner->layers |= node->owner->layers;
			}
		}
		/* Schedule parent nodes. */
		foreach (DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEG_NODE_TYPE_OPERATION) {
				OperationDepsNode *from = (OperationDepsNode *)rel->from;
				if ((rel->flag & DEPSREL_FLAG_CYCLIC) == 0) {
					BLI_assert(from->num_links_pending > 0);
					--from->num_links_pending;
				}
				if (from->num_links_pending == 0 && from->done == 0) {
					BLI_stack_push(stack, &from);
					from->done = 1;
				}
			}
		}
	}
	BLI_stack_free(stack);
}

void deg_graph_build_finalize(Depsgraph *graph)
{
	/* STEP 1: Make sure new invisible dependencies are ready for use.
	 *
	 * TODO(sergey): This might do a bit of extra tagging, but it's kinda nice
	 * to do it ahead of a time and don't spend time on flushing updates on
	 * every frame change.
	 */
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		if (id_node->layers == 0) {
			ID *id = id_node->id;
			if (GS(id->name) == ID_OB) {
				Object *object = (Object *)id;
				if (check_object_needs_evaluation(object)) {
					id_node->tag_update(graph);
				}
			}
		}
	}
	/* STEP 2: Flush visibility layers from children to parent. */
	deg_graph_build_flush_layers(graph);
	/* STEP 3: Re-tag IDs for update if it was tagged before the relations
	 * update tag.
	 */
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp, id_node->components)
		{
			id_node->layers |= comp->layers;
		}
		GHASH_FOREACH_END();

		if ((id_node->layers & graph->layers) != 0 || graph->layers == 0) {
			ID *id = id_node->id;
			if ((id->recalc & ID_RECALC_ALL) &&
			    (id->tag & LIB_TAG_DOIT))
			{
				id_node->tag_update(graph);
				id->tag &= ~LIB_TAG_DOIT;
			}
			else if (GS(id->name) == ID_OB) {
				Object *object = (Object *)id;
				if (object->recalc & OB_RECALC_ALL) {
					id_node->tag_update(graph);
					id->tag &= ~LIB_TAG_DOIT;
				}
			}
		}
		id_node->finalize_build();
	}
}

}  // namespace DEG
