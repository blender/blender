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

extern "C" {
#include "BKE_animsys.h"
}

#include "intern/depsgraph.h"
#include "intern/depsgraph_types.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "util/deg_util_foreach.h"

#include "DEG_depsgraph.h"

namespace DEG {

namespace {

void deg_graph_build_flush_visibility(Depsgraph *graph)
{
	enum {
		DEG_NODE_VISITED = (1 << 0),
	};

	BLI_Stack *stack = BLI_stack_new(sizeof(OperationDepsNode *),
	                                 "DEG flush layers stack");
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, id_node->components)
		{
			comp_node->affects_directly_visible |= id_node->is_directly_visible;
		}
		GHASH_FOREACH_END();
	}
	foreach (OperationDepsNode *op_node, graph->operations) {
		op_node->custom_flags = 0;
		op_node->num_links_pending = 0;
		foreach (DepsRelation *rel, op_node->outlinks) {
			if ((rel->from->type == DEG_NODE_TYPE_OPERATION) &&
			    (rel->flag & DEPSREL_FLAG_CYCLIC) == 0)
			{
				++op_node->num_links_pending;
			}
		}
		if (op_node->num_links_pending == 0) {
			BLI_stack_push(stack, &op_node);
			op_node->custom_flags |= DEG_NODE_VISITED;
		}
	}
	while (!BLI_stack_is_empty(stack)) {
		OperationDepsNode *op_node;
		BLI_stack_pop(stack, &op_node);
		/* Flush layers to parents. */
		foreach (DepsRelation *rel, op_node->inlinks) {
			if (rel->from->type == DEG_NODE_TYPE_OPERATION) {
				OperationDepsNode *op_from = (OperationDepsNode *)rel->from;
				op_from->owner->affects_directly_visible |=
				        op_node->owner->affects_directly_visible;
			}
		}
		/* Schedule parent nodes. */
		foreach (DepsRelation *rel, op_node->inlinks) {
			if (rel->from->type == DEG_NODE_TYPE_OPERATION) {
				OperationDepsNode *op_from = (OperationDepsNode *)rel->from;
				if ((rel->flag & DEPSREL_FLAG_CYCLIC) == 0) {
					BLI_assert(op_from->num_links_pending > 0);
					--op_from->num_links_pending;
				}
				if ((op_from->num_links_pending == 0) &&
				    (op_from->custom_flags & DEG_NODE_VISITED) == 0)
				{
					BLI_stack_push(stack, &op_from);
					op_from->custom_flags |= DEG_NODE_VISITED;
				}
			}
		}
	}
	BLI_stack_free(stack);
}

}  // namespace

void deg_graph_build_finalize(Main *bmain, Depsgraph *graph)
{
	/* Make sure dependencies of visible ID datablocks are visible. */
	deg_graph_build_flush_visibility(graph);
	/* Re-tag IDs for update if it was tagged before the relations
	 * update tag.
	 */
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		ID *id = id_node->id_orig;
		id_node->finalize_build(graph);
		int flag = 0;
		if ((id->recalc & ID_RECALC_ALL)) {
			AnimData *adt = BKE_animdata_from_id(id);
			if (adt != NULL && (adt->recalc & ADT_RECALC_ANIM) != 0) {
				flag |= DEG_TAG_TIME;
			}
		}
		/* Tag rebuild if special evaluation flags changed. */
		if (id_node->eval_flags != id_node->previous_eval_flags) {
			flag |= DEG_TAG_TRANSFORM | DEG_TAG_GEOMETRY;
		}
		if (!deg_copy_on_write_is_expanded(id_node->id_cow)) {
			flag |= DEG_TAG_COPY_ON_WRITE;
			/* This means ID is being added to the dependency graph first
			 * time, which is similar to "ob-visible-change"
			 */
			if (GS(id->name) == ID_OB) {
				flag |= OB_RECALC_OB | OB_RECALC_DATA;
			}
		}
		if (flag != 0) {
			DEG_graph_id_tag_update(bmain,
			                        (::Depsgraph *)graph,
			                        id_node->id_orig,
			                        flag);
		}
	}
}

}  // namespace DEG
