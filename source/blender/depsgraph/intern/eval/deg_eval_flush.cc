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
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/depsgraph_tag.cc
 *  \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#include "intern/eval/deg_eval_flush.h"

// TODO(sergey): Use some sort of wrapper.
#include <queue>

extern "C" {
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_task.h"
#include "BLI_ghash.h"

#include "DEG_depsgraph.h"
} /* extern "C" */

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

namespace DEG {

namespace {

// TODO(sergey): De-duplicate with depsgraph_tag,cc
void lib_id_recalc_tag(Main *bmain, ID *id)
{
	id->tag |= LIB_TAG_ID_RECALC;
	DEG_id_type_tag(bmain, GS(id->name));
}

void lib_id_recalc_data_tag(Main *bmain, ID *id)
{
	id->tag |= LIB_TAG_ID_RECALC_DATA;
	DEG_id_type_tag(bmain, GS(id->name));
}

}  /* namespace */

typedef std::queue<OperationDepsNode *> FlushQueue;

static void flush_init_func(void *data_v, int i)
{
	/* ID node's done flag is used to avoid multiple editors update
	 * for the same ID.
	 */
	Depsgraph *graph = (Depsgraph *)data_v;
	OperationDepsNode *node = graph->operations[i];
	IDDepsNode *id_node = node->owner->owner;
	id_node->done = 0;
	node->scheduled = false;
	node->owner->flags &= ~DEPSCOMP_FULLY_SCHEDULED;
	if (node->owner->type == DEPSNODE_TYPE_PROXY) {
		node->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
	}
}

/* Flush updates from tagged nodes outwards until all affected nodes
 * are tagged.
 */
void deg_graph_flush_updates(Main *bmain, Depsgraph *graph)
{
	/* Sanity check. */
	if (graph == NULL) {
		return;
	}

	/* Nothing to update, early out. */
	if (BLI_gset_size(graph->entry_tags) == 0) {
		return;
	}

	/* TODO(sergey): With a bit of flag magic we can get rid of this
	 * extra loop.
	 */
	const int num_operations = graph->operations.size();
	const bool do_threads = num_operations > 256;
	BLI_task_parallel_range(0,
	                        num_operations,
	                        graph,
	                        flush_init_func,
	                        do_threads);

	FlushQueue queue;
	/* Starting from the tagged "entry" nodes, flush outwards... */
	/* NOTE: Also need to ensure that for each of these, there is a path back to
	 *       root, or else they won't be done.
	 * NOTE: Count how many nodes we need to handle - entry nodes may be
	 *       component nodes which don't count for this purpose!
	 */
	GSET_FOREACH_BEGIN(OperationDepsNode *, node, graph->entry_tags)
	{
		queue.push(node);
		node->scheduled = true;
	}
	GSET_FOREACH_END();

	while (!queue.empty()) {
		OperationDepsNode *node = queue.front();
		queue.pop();

		for (;;) {
			node->flag |= DEPSOP_FLAG_NEEDS_UPDATE;

			IDDepsNode *id_node = node->owner->owner;

			if (id_node->done == 0) {
				deg_editors_id_update(bmain, id_node->id);
				id_node->done = 1;
			}

			lib_id_recalc_tag(bmain, id_node->id);
			/* TODO(sergey): For until we've got proper data nodes in the graph. */
			lib_id_recalc_data_tag(bmain, id_node->id);

			ID *id = id_node->id;
			/* This code is used to preserve those areas which does direct
			 * object update,
			 *
			 * Plus it ensures visibility changes and relations and layers
			 * visibility update has proper flags to work with.
			 */
			if (GS(id->name) == ID_OB) {
				Object *object = (Object *)id;
				ComponentDepsNode *comp_node = node->owner;
				if (comp_node->type == DEPSNODE_TYPE_ANIMATION) {
					object->recalc |= OB_RECALC_TIME;
				}
				else if (comp_node->type == DEPSNODE_TYPE_TRANSFORM) {
					object->recalc |= OB_RECALC_OB;
				}
				else {
					object->recalc |= OB_RECALC_DATA;
				}
			}

			/* TODO(sergey): For until incremental updates are possible
			 * witin a component at least we tag the whole component
			 * for update.
			 */
			ComponentDepsNode *component = node->owner;
			if ((component->flags & DEPSCOMP_FULLY_SCHEDULED) == 0) {
				foreach (OperationDepsNode *op, component->operations) {
					op->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
				}
				component->flags |= DEPSCOMP_FULLY_SCHEDULED;
			}

			/* Flush to nodes along links... */
			if (node->outlinks.size() == 1) {
				OperationDepsNode *to_node = (OperationDepsNode *)node->outlinks[0]->to;
				if (to_node->scheduled == false) {
					to_node->scheduled = true;
					node = to_node;
				}
				else {
					break;
				}
			}
			else {
				foreach (DepsRelation *rel, node->outlinks) {
					OperationDepsNode *to_node = (OperationDepsNode *)rel->to;
					if (to_node->scheduled == false) {
						queue.push(to_node);
						to_node->scheduled = true;
					}
				}
				break;
			}
		}
	}
}

static void graph_clear_func(void *data_v, int i)
{
	Depsgraph *graph = (Depsgraph *)data_v;
	OperationDepsNode *node = graph->operations[i];
	/* Clear node's "pending update" settings. */
	node->flag &= ~(DEPSOP_FLAG_DIRECTLY_MODIFIED | DEPSOP_FLAG_NEEDS_UPDATE);
}

/* Clear tags from all operation nodes. */
void deg_graph_clear_tags(Depsgraph *graph)
{
	/* Go over all operation nodes, clearing tags. */
	const int num_operations = graph->operations.size();
	const bool do_threads = num_operations > 256;
	BLI_task_parallel_range(0, num_operations, graph, graph_clear_func, do_threads);
	/* Clear any entry tags which haven't been flushed. */
	BLI_gset_clear(graph->entry_tags, NULL);
}

}  // namespace DEG
