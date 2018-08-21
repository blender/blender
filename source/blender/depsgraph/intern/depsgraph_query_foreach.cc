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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_query_foreach.cc
 *  \ingroup depsgraph
 *
 * Implementation of Querying and Filtering API's
 */

// TODO(sergey): Use some sort of wrapper.
#include <deque>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
} /* extern "C" */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/depsgraph_intern.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"

#include "util/deg_util_foreach.h"

/* ************************ DEG TRAVERSAL ********************* */

namespace DEG {

typedef std::deque<OperationDepsNode *> TraversalQueue;

static void deg_foreach_clear_flags(const Depsgraph *graph)
{
	foreach (OperationDepsNode *op_node, graph->operations) {
		op_node->scheduled = false;
	}
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		id_node->done = false;
	}
}

static void deg_foreach_dependent_ID(const Depsgraph *graph,
                                     const ID *id,
                                     DEGForeachIDCallback callback,
                                     void *user_data)
{
	/* Start with getting ID node from the graph. */
	IDDepsNode *target_id_node = graph->find_id_node(id);
	if (target_id_node == NULL) {
		/* TODO(sergey): Shall we inform or assert here about attempt to start
		 * iterating over non-existing ID?
		 */
		return;
	}
	/* Make sure all runtime flags are ready and clear. */
	deg_foreach_clear_flags(graph);
	/* Start with scheduling all operations from ID node. */
	TraversalQueue queue;
	GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, target_id_node->components)
	{
		foreach (OperationDepsNode *op_node, comp_node->operations) {
			queue.push_back(op_node);
			op_node->scheduled = true;
		}
	}
	GHASH_FOREACH_END();
	target_id_node->done = true;
	/* Process the queue. */
	while (!queue.empty()) {
		/* get next operation node to process. */
		OperationDepsNode *op_node = queue.front();
		queue.pop_front();
		for (;;) {
			/* Check whether we need to inform callee about corresponding ID node. */
			ComponentDepsNode *comp_node = op_node->owner;
			IDDepsNode *id_node = comp_node->owner;
			if (!id_node->done) {
				/* TODO(sergey): Is it orig or CoW? */
				callback(id_node->id_orig, user_data);
				id_node->done = true;
			}
			/* Schedule outgoing operation nodes. */
			if (op_node->outlinks.size() == 1) {
				OperationDepsNode *to_node = (OperationDepsNode *)op_node->outlinks[0]->to;
				if (to_node->scheduled == false) {
					to_node->scheduled = true;
					op_node = to_node;
				}
				else {
					break;
				}
			}
			else {
				foreach (DepsRelation *rel, op_node->outlinks) {
					OperationDepsNode *to_node = (OperationDepsNode *)rel->to;
					if (to_node->scheduled == false) {
						queue.push_front(to_node);
						to_node->scheduled = true;
					}
				}
				break;
			}
		}
	}
}

static void deg_foreach_id(const Depsgraph *depsgraph,
                           DEGForeachIDCallback callback, void *user_data)
{
	foreach (const IDDepsNode *id_node, depsgraph->id_nodes) {
		callback(id_node->id_orig, user_data);
	}
}

}  // namespace DEG

void DEG_foreach_dependent_ID(const Depsgraph *depsgraph,
                              const ID *id,
                              DEGForeachIDCallback callback, void *user_data)
{
	DEG::deg_foreach_dependent_ID((const DEG::Depsgraph *)depsgraph,
	                              id,
	                              callback, user_data);
}

void DEG_foreach_ID(const Depsgraph *depsgraph,
                    DEGForeachIDCallback callback, void *user_data)
{
	DEG::deg_foreach_id((const DEG::Depsgraph *)depsgraph, callback, user_data);
}
