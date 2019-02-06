/*
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
 */

/** \file \ingroup depsgraph
 */

#include "intern/builder/deg_builder_transitive.h"

#include "MEM_guardedalloc.h"

#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_operation.h"

#include "intern/depsgraph.h"
#include "intern/debug/deg_debug.h"

namespace DEG {

/* -------------------------------------------------- */

/* Performs a transitive reduction to remove redundant relations.
 * https://en.wikipedia.org/wiki/Transitive_reduction
 *
 * XXX The current implementation is somewhat naive and has O(V*E) worst case
 * runtime.
 * A more optimized algorithm can be implemented later, e.g.
 *
 *   http://www.sciencedirect.com/science/article/pii/0304397588900321/pdf?md5=3391e309b708b6f9cdedcd08f84f4afc&pid=1-s2.0-0304397588900321-main.pdf
 *
 * Care has to be taken to make sure the algorithm can handle the cyclic case
 * too! (unless we can to prevent this case early on).
 */

enum {
	OP_VISITED = 1,
	OP_REACHABLE = 2,
};

static void deg_graph_tag_paths_recursive(Node *node)
{
	if (node->custom_flags & OP_VISITED) {
		return;
	}
	node->custom_flags |= OP_VISITED;
	for (Relation *rel : node->inlinks) {
		deg_graph_tag_paths_recursive(rel->from);
		/* Do this only in inlinks loop, so the target node does not get
		 * flagged. */
		rel->from->custom_flags |= OP_REACHABLE;
	}
}

void deg_graph_transitive_reduction(Depsgraph *graph)
{
	int num_removed_relations = 0;
	for (OperationNode *target : graph->operations) {
		/* Clear tags. */
		for (OperationNode *node : graph->operations) {
			node->custom_flags = 0;
		}
		/* Mark nodes from which we can reach the target
		 * start with children, so the target node and direct children are not
		 * flagged. */
		target->custom_flags |= OP_VISITED;
		for (Relation *rel : target->inlinks) {
			deg_graph_tag_paths_recursive(rel->from);
		}
		/* Remove redundant paths to the target. */
		for (Node::Relations::const_iterator it_rel = target->inlinks.begin();
		     it_rel != target->inlinks.end();
		     )
		{
			Relation *rel = *it_rel;
			if (rel->from->type == NodeType::TIMESOURCE) {
				/* HACK: time source nodes don't get "custom_flags" flag
				 * set/cleared. */
				/* TODO: there will be other types in future, so iterators above
				 * need modifying. */
				++it_rel;
			}
			else if (rel->from->custom_flags & OP_REACHABLE) {
				rel->unlink();
				OBJECT_GUARDED_DELETE(rel, Relation);
				++num_removed_relations;
			}
			else {
				++it_rel;
			}
		}
	}
	DEG_DEBUG_PRINTF((::Depsgraph *)graph, BUILD, "Removed %d relations\n", num_removed_relations);
}

}  // namespace DEG
