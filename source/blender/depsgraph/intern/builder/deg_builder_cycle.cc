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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder_cycle.cc
 *  \ingroup depsgraph
 */

#include "intern/builder/deg_builder_cycle.h"

// TOO(sergey): Use some wrappers over those?
#include <cstdio>
#include <cstdlib>

#include "BLI_utildefines.h"
#include "BLI_stack.h"

#include "util/deg_util_foreach.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph.h"

namespace DEG {

void deg_graph_detect_cycles(Depsgraph *graph)
{
	enum {
		/* Not is not visited at all during traversal. */
		NODE_NOT_VISITED = 0,
		/* Node has been visited during traversal and not in current stack. */
		NODE_VISITED = 1,
		/* Node has been visited during traversal and is in current stack. */
		NODE_IN_STACK = 2,
	};

	struct StackEntry {
		OperationDepsNode *node;
		StackEntry *from;
		DepsRelation *via_relation;
	};

	BLI_Stack *traversal_stack = BLI_stack_new(sizeof(StackEntry),
	                                           "DEG detect cycles stack");

	foreach (OperationDepsNode *node, graph->operations) {
		bool has_inlinks = false;
		foreach (DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEG_NODE_TYPE_OPERATION) {
				has_inlinks = true;
			}
		}
		if (has_inlinks == false) {
			StackEntry entry;
			entry.node = node;
			entry.from = NULL;
			entry.via_relation = NULL;
			BLI_stack_push(traversal_stack, &entry);
			node->tag = NODE_IN_STACK;
		}
		else {
			node->tag = NODE_NOT_VISITED;
		}
		node->done = 0;
	}

	while (!BLI_stack_is_empty(traversal_stack)) {
		StackEntry *entry = (StackEntry *)BLI_stack_peek(traversal_stack);
		OperationDepsNode *node = entry->node;
		bool all_child_traversed = true;
		for (int i = node->done; i < node->outlinks.size(); ++i) {
			DepsRelation *rel = node->outlinks[i];
			if (rel->to->type == DEG_NODE_TYPE_OPERATION) {
				OperationDepsNode *to = (OperationDepsNode *)rel->to;
				if (to->tag == NODE_IN_STACK) {
					printf("Dependency cycle detected:\n");
					printf("  '%s' depends on '%s' through '%s'\n",
					       to->full_identifier().c_str(),
					       node->full_identifier().c_str(),
					       rel->name);

					StackEntry *current = entry;
					while (current->node != to) {
						BLI_assert(current != NULL);
						printf("  '%s' depends on '%s' through '%s'\n",
						       current->node->full_identifier().c_str(),
						       current->from->node->full_identifier().c_str(),
						       current->via_relation->name);
						current = current->from;
					}
					/* TODO(sergey): So called russian roulette cycle solver. */
					rel->flag |= DEPSREL_FLAG_CYCLIC;
				}
				else if (to->tag == NODE_NOT_VISITED) {
					StackEntry new_entry;
					new_entry.node = to;
					new_entry.from = entry;
					new_entry.via_relation = rel;
					BLI_stack_push(traversal_stack, &new_entry);
					to->tag = NODE_IN_STACK;
					all_child_traversed = false;
					node->done = i;
					break;
				}
			}
		}
		if (all_child_traversed) {
			node->tag = NODE_VISITED;
			BLI_stack_discard(traversal_stack);
		}
	}

	BLI_stack_free(traversal_stack);
}

}  // namespace DEG
