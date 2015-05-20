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
 * Contributor(s): Lukas Toenne,
 *                 Sergey Sharybin,
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/util/depsgraph_util_transitive.cc
 *  \ingroup depsgraph
 */

extern "C" {
#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_ID.h"

#include "RNA_access.h"
#include "RNA_types.h"
}

#include "depsgraph_util_transitive.h"
#include "depsgraph.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"

/* -------------------------------------------------- */

/* Performs a transitive reduction to remove redundant relations.
 * http://en.wikipedia.org/wiki/Transitive_reduction
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

static void deg_graph_tag_paths_recursive(DepsNode *node)
{
	if (node->done & OP_VISITED)
		return;
	node->done |= OP_VISITED;

	for (OperationDepsNode::Relations::const_iterator it = node->inlinks.begin();
	     it != node->inlinks.end();
	     ++it)
	{
		DepsRelation *rel = *it;

		deg_graph_tag_paths_recursive(rel->from);
		/* Do this only in inlinks loop, so the target node does not get
		 * flagged.
		 */
		rel->from->done |= OP_REACHABLE;
	}
}

void deg_graph_transitive_reduction(Depsgraph *graph)
{
	for (Depsgraph::OperationNodes::const_iterator it_target = graph->operations.begin();
	     it_target != graph->operations.end();
	     ++it_target)
	{
		OperationDepsNode *target = *it_target;

		/* Clear tags. */
		for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin();
		     it != graph->operations.end();
		     ++it)
		{
			OperationDepsNode *node = *it;
			node->done = 0;
		}

		/* mark nodes from which we can reach the target
		 * start with children, so the target node and direct children are not
		 * flagged.
		 */
		target->done |= OP_VISITED;
		for (OperationDepsNode::Relations::const_iterator it = target->inlinks.begin();
		     it != target->inlinks.end();
		     ++it)
		{
			DepsRelation *rel = *it;

			deg_graph_tag_paths_recursive(rel->from);
		}

		/* Eemove redundant paths to the target. */
		for (DepsNode::Relations::const_iterator it_rel = target->inlinks.begin();
		     it_rel != target->inlinks.end();
		     )
		{
			DepsRelation *rel = *it_rel;
			/* Increment in advance, so we can safely remove the relation. */
			++it_rel;

			if (rel->from->type == DEPSNODE_TYPE_TIMESOURCE) {
				/* HACK: time source nodes don't get "done" flag set/cleared. */
				/* TODO: there will be other types in future, so iterators above
				 * need modifying.
				 */
			}
			else if (rel->from->done & OP_REACHABLE) {
				OBJECT_GUARDED_DELETE(rel, DepsRelation);
			}
		}
	}
}
