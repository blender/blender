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

/** \file blender/depsgraph/intern/eval/deg_eval_flush.cc
 *  \ingroup depsgraph
 *
 * Core routines for how the Depsgraph works.
 */

#include "intern/eval/deg_eval_flush.h"

// TODO(sergey): Use some sort of wrapper.
#include <deque>

#include "BLI_utildefines.h"
#include "BLI_task.h"
#include "BLI_ghash.h"

extern "C" {
#include "DNA_object_types.h"
} /* extern "C" */

#include "DEG_depsgraph.h"

#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

namespace DEG {

enum {
	COMPONENT_STATE_NONE      = 0,
	COMPONENT_STATE_SCHEDULED = 1,
	COMPONENT_STATE_DONE      = 2,
};

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

typedef std::deque<OperationDepsNode *> FlushQueue;

static void flush_init_func(void *data_v, int i)
{
	/* ID node's done flag is used to avoid multiple editors update
	 * for the same ID.
	 */
	Depsgraph *graph = (Depsgraph *)data_v;
	OperationDepsNode *node = graph->operations[i];
	ComponentDepsNode *comp_node = node->owner;
	IDDepsNode *id_node = comp_node->owner;
	id_node->done = 0;
	comp_node->done = COMPONENT_STATE_NONE;
	node->scheduled = false;
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
		queue.push_back(node);
		node->scheduled = true;
	}
	GSET_FOREACH_END();

	int num_flushed_objects = 0;
	while (!queue.empty()) {
		OperationDepsNode *node = queue.front();
		queue.pop_front();

		for (;;) {
			node->flag |= DEPSOP_FLAG_NEEDS_UPDATE;

			ComponentDepsNode *comp_node = node->owner;
			IDDepsNode *id_node = comp_node->owner;

			ID *id = id_node->id;
			if (id_node->done == 0) {
				deg_editors_id_update(bmain, id);
				lib_id_recalc_tag(bmain, id);
				/* TODO(sergey): For until we've got proper data nodes in the graph. */
				lib_id_recalc_data_tag(bmain, id);
			}

			if (comp_node->done != COMPONENT_STATE_DONE) {
				Object *object = NULL;
				if (GS(id->name) == ID_OB) {
					object = (Object *)id;
					if (id_node->done == 0) {
						++num_flushed_objects;
					}
				}
				foreach (OperationDepsNode *op, comp_node->operations) {
					op->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
				}
				if (object != NULL) {
					/* This code is used to preserve those areas which does
					 * direct object update,
					 *
					 * Plus it ensures visibility changes and relations and
					 * layers visibility update has proper flags to work with.
					 */
					switch (comp_node->type) {
						case DEG_NODE_TYPE_UNDEFINED:
						case DEG_NODE_TYPE_OPERATION:
						case DEG_NODE_TYPE_TIMESOURCE:
						case DEG_NODE_TYPE_ID_REF:
						case DEG_NODE_TYPE_PARAMETERS:
						case DEG_NODE_TYPE_SEQUENCER:
							/* Ignore, does not translate to object component. */
							break;
						case DEG_NODE_TYPE_ANIMATION:
							object->recalc |= OB_RECALC_TIME;
							break;
						case DEG_NODE_TYPE_TRANSFORM:
							object->recalc |= OB_RECALC_OB;
							break;
						case DEG_NODE_TYPE_GEOMETRY:
						case DEG_NODE_TYPE_EVAL_POSE:
						case DEG_NODE_TYPE_BONE:
						case DEG_NODE_TYPE_EVAL_PARTICLES:
						case DEG_NODE_TYPE_SHADING:
						case DEG_NODE_TYPE_CACHE:
						case DEG_NODE_TYPE_PROXY:
							object->recalc |= OB_RECALC_DATA;
							break;
					}
				}
				/* When some target changes bone, we might need to re-run the
				 * whole IK solver, otherwise result might be unpredictable.
				 */
				if (comp_node->type == DEG_NODE_TYPE_BONE) {
					ComponentDepsNode *pose_comp =
					        id_node->find_component(DEG_NODE_TYPE_EVAL_POSE);
					BLI_assert(pose_comp != NULL);
					if (pose_comp->done == COMPONENT_STATE_NONE) {
						queue.push_front(pose_comp->get_entry_operation());
						pose_comp->done = COMPONENT_STATE_SCHEDULED;
					}
				}
			}

			id_node->done = 1;
			comp_node->done = COMPONENT_STATE_DONE;

			/* Flush to nodes along links... */
			/* TODO(sergey): This is mainly giving speedup due ot less queue pushes, which
			 * reduces number of memory allocations.
			 *
			 * We should try solve the allocation issue instead of doing crazy things here.
			 */
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
						queue.push_front(to_node);
						to_node->scheduled = true;
					}
				}
				break;
			}
		}
	}
	DEG_DEBUG_PRINTF("Update flushed to %d objects\n", num_flushed_objects);
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
