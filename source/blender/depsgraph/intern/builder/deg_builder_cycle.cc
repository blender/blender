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

namespace {

typedef enum eCyclicCheckVisitedState {
	/* Not is not visited at all during traversal. */
	NODE_NOT_VISITED = 0,
	/* Node has been visited during traversal and not in current stack. */
	NODE_VISITED = 1,
	/* Node has been visited during traversal and is in current stack. */
	NODE_IN_STACK = 2,
} eCyclicCheckVisitedState;

struct StackEntry {
	OperationDepsNode *node;
	StackEntry *from;
	DepsRelation *via_relation;
};

struct CyclesSolverState {
	CyclesSolverState(Depsgraph *graph)
		: graph(graph),
		  traversal_stack(BLI_stack_new(sizeof(StackEntry),
		                                "DEG detect cycles stack")),
		  num_cycles(0)
	{
		/* pass */
	}
	~CyclesSolverState() {
		BLI_stack_free(traversal_stack);
		if (num_cycles != 0) {
			printf("Detected %d dependency cycles\n", num_cycles);
		}
	}
	Depsgraph *graph;
	BLI_Stack *traversal_stack;
	int num_cycles;
};

BLI_INLINE void set_node_visited_state(DepsNode *node,
                                       eCyclicCheckVisitedState state)
{
	node->custom_flags = (node->custom_flags & ~0x3) | (int)state;
}

BLI_INLINE eCyclicCheckVisitedState get_node_visited_state(DepsNode *node)
{
	return (eCyclicCheckVisitedState)(node->custom_flags & 0x3);
}

BLI_INLINE void set_node_num_visited_children(DepsNode *node, int num_children)
{
	node->custom_flags = (node->custom_flags & 0x3) | (num_children << 2);
}

BLI_INLINE int get_node_num_visited_children(DepsNode *node)
{
	return node->custom_flags >> 2;
}

void schedule_node_to_stack(CyclesSolverState *state, OperationDepsNode *node)
{
	StackEntry entry;
	entry.node = node;
	entry.from = NULL;
	entry.via_relation = NULL;
	BLI_stack_push(state->traversal_stack, &entry);
	set_node_visited_state(node, NODE_IN_STACK);
}

/* Schedule leaf nodes (node without input links) for traversal. */
void schedule_leaf_nodes(CyclesSolverState *state)
{
	foreach (OperationDepsNode *node, state->graph->operations) {
		bool has_inlinks = false;
		foreach (DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEG_NODE_TYPE_OPERATION) {
				has_inlinks = true;
			}
		}
		node->custom_flags = 0;
		if (has_inlinks == false) {
			schedule_node_to_stack(state, node);
		}
		else {
			set_node_visited_state(node, NODE_NOT_VISITED);
		}
	}
}

/* Schedule node which was not checked yet for being belong to
 * any of dependency cycle.
 */
bool schedule_non_checked_node(CyclesSolverState *state)
{
	foreach (OperationDepsNode *node, state->graph->operations) {
		if (get_node_visited_state(node) == NODE_NOT_VISITED) {
			schedule_node_to_stack(state, node);
			return true;
		}
	}
	return false;
}

/* Solve cycles with all nodes which are scheduled for traversal. */
void solve_cycles(CyclesSolverState *state)
{
	BLI_Stack *traversal_stack = state->traversal_stack;
	while (!BLI_stack_is_empty(traversal_stack)) {
		StackEntry *entry = (StackEntry *)BLI_stack_peek(traversal_stack);
		OperationDepsNode *node = entry->node;
		bool all_child_traversed = true;
		const int num_visited = get_node_num_visited_children(node);
		for (int i = num_visited; i < node->outlinks.size(); ++i) {
			DepsRelation *rel = node->outlinks[i];
			if (rel->to->type == DEG_NODE_TYPE_OPERATION) {
				OperationDepsNode *to = (OperationDepsNode *)rel->to;
				eCyclicCheckVisitedState to_state = get_node_visited_state(to);
				if (to_state == NODE_IN_STACK) {
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
					++state->num_cycles;
				}
				else if (to_state == NODE_NOT_VISITED) {
					StackEntry new_entry;
					new_entry.node = to;
					new_entry.from = entry;
					new_entry.via_relation = rel;
					BLI_stack_push(traversal_stack, &new_entry);
					set_node_visited_state(node, NODE_IN_STACK);
					all_child_traversed = false;
					set_node_num_visited_children(node, i);
					break;
				}
			}
		}
		if (all_child_traversed) {
			set_node_visited_state(node, NODE_VISITED);
			BLI_stack_discard(traversal_stack);
		}
	}
}

}  // namespace

void deg_graph_detect_cycles(Depsgraph *graph)
{
	CyclesSolverState state(graph);
	/* First we solve cycles which are reachable from leaf nodes. */
	schedule_leaf_nodes(&state);
	solve_cycles(&state);
	/* We are not done yet. It is possible to have closed loop cycle,
	 * for example A -> B -> C -> A. These nodes were not scheduled
	 * yet (since they all have inlinks), and were not traversed since
	 * nobody else points to them.
	 */
	while (schedule_non_checked_node(&state)) {
		solve_cycles(&state);
	}
}

}  // namespace DEG
