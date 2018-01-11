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
#include "intern/nodes/deg_node_id.h"
#include "intern/nodes/deg_node_operation.h"

#include "intern/depsgraph_intern.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "util/deg_util_foreach.h"

namespace DEG {

enum {
	ID_STATE_NONE     = 0,
	ID_STATE_MODIFIED = 1,
};

enum {
	COMPONENT_STATE_NONE      = 0,
	COMPONENT_STATE_SCHEDULED = 1,
	COMPONENT_STATE_DONE      = 2,
};

typedef std::deque<OperationDepsNode *> FlushQueue;

namespace {

void flush_init_operation_node_func(
        void *__restrict data_v,
        const int i,
        const ParallelRangeTLS *__restrict /*tls*/)
{
	Depsgraph *graph = (Depsgraph *)data_v;
	OperationDepsNode *node = graph->operations[i];
	node->scheduled = false;
}

void flush_init_id_node_func(
        void *__restrict data_v,
        const int i,
        const ParallelRangeTLS *__restrict /*tls*/)
{
	Depsgraph *graph = (Depsgraph *)data_v;
	IDDepsNode *id_node = graph->id_nodes[i];
	id_node->done = ID_STATE_NONE;
	GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, id_node->components)
		comp_node->done = COMPONENT_STATE_NONE;
	GHASH_FOREACH_END();
}

BLI_INLINE void flush_prepare(Depsgraph *graph)
{
	{
		const int num_operations = graph->operations.size();
		ParallelRangeSettings settings;
		BLI_parallel_range_settings_defaults(&settings);
		settings.min_iter_per_thread = 1024;
		BLI_task_parallel_range(0, num_operations,
		                        graph,
		                        flush_init_operation_node_func,
		                        &settings);
	}
	{
		const int num_id_nodes = graph->id_nodes.size();
		ParallelRangeSettings settings;
		BLI_parallel_range_settings_defaults(&settings);
		settings.min_iter_per_thread = 1024;
		BLI_task_parallel_range(0, num_id_nodes,
		                        graph,
		                        flush_init_id_node_func,
		                        &settings);
	}
}

BLI_INLINE void flush_schedule_entrypoints(Depsgraph *graph, FlushQueue *queue)
{
	GSET_FOREACH_BEGIN(OperationDepsNode *, op_node, graph->entry_tags)
	{
		queue->push_back(op_node);
		op_node->scheduled = true;
	}
	GSET_FOREACH_END();
}

BLI_INLINE void flush_handle_id_node(IDDepsNode *id_node)
{
	id_node->done = ID_STATE_MODIFIED;
}

/* TODO(sergey): We can reduce number of arguments here. */
BLI_INLINE void flush_handle_component_node(Depsgraph *graph,
                                            IDDepsNode *id_node,
                                            ComponentDepsNode *comp_node,
                                            bool use_copy_on_write,
                                            FlushQueue *queue)
{
	/* We only handle component once. */
	if (comp_node->done == COMPONENT_STATE_DONE) {
		return;
	}
	comp_node->done = COMPONENT_STATE_DONE;
	/* Currently this is needed to get object->mesh to be replaced with
	 * original mesh (rather than being evaluated_mesh).
	 *
	 * TODO(sergey): This is something we need to avoid.
	 */
	if (use_copy_on_write && comp_node->depends_on_cow()) {
		ComponentDepsNode *cow_comp =
		        id_node->find_component(DEG_NODE_TYPE_COPY_ON_WRITE);
		cow_comp->tag_update(graph);
	}
	/* Tag all required operations in component for update.  */
	foreach (OperationDepsNode *op, comp_node->operations) {
		/* We don't want to flush tags in "upstream" direction for
		 * certain types of operations.
		 *
		 * TODO(sergey): Need a more generic solution for this.
		 */
		if (op->opcode == DEG_OPCODE_PARTICLE_SETTINGS_EVAL) {
			continue;
		}
		op->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
	}
	/* When some target changes bone, we might need to re-run the
	 * whole IK solver, otherwise result might be unpredictable.
	 */
	if (comp_node->type == DEG_NODE_TYPE_BONE) {
		ComponentDepsNode *pose_comp =
		        id_node->find_component(DEG_NODE_TYPE_EVAL_POSE);
		BLI_assert(pose_comp != NULL);
		if (pose_comp->done == COMPONENT_STATE_NONE) {
			queue->push_front(pose_comp->get_entry_operation());
			pose_comp->done = COMPONENT_STATE_SCHEDULED;
		}
	}
}

/* Schedule children of the given operation node for traversal.
 *
 * One of the children will by-pass the queue and will be returned as a function
 * return value, so it can start being handled right away, without building too
 * much of a queue.
 */
BLI_INLINE OperationDepsNode *flush_schedule_children(
        OperationDepsNode *op_node,
        FlushQueue *queue)
{
	OperationDepsNode *result = NULL;
	foreach (DepsRelation *rel, op_node->outlinks) {
		OperationDepsNode *to_node = (OperationDepsNode *)rel->to;
		if (to_node->scheduled == false) {
			if (result != NULL) {
				queue->push_front(to_node);
			}
			else {
				result = to_node;
			}
			to_node->scheduled = true;
		}
	}
	return result;
}

/* NOTE: It will also accumulate flags from changed components. */
BLI_INLINE void flush_editors_id_update(Main *bmain,
                                        Depsgraph *graph,
                                        const DEGEditorUpdateContext *update_ctx)
{
	foreach (IDDepsNode *id_node, graph->id_nodes) {
		if (id_node->done != ID_STATE_MODIFIED) {
			continue;
		}
		DEG_id_type_tag(bmain, GS(id_node->id_orig->name));
		/* TODO(sergey): Do we need to pass original or evaluated ID here? */
		ID *id_orig = id_node->id_orig;
		ID *id_cow = id_node->id_cow;
		/* Copy tag from original data to CoW storage.
		 * This is because DEG_id_tag_update() sets tags on original
		 * data.
		 */
		id_cow->recalc |= (id_orig->recalc & ID_RECALC_ALL);
		/* Gather recalc flags from all changed components. */
		GHASH_FOREACH_BEGIN(ComponentDepsNode *, comp_node, id_node->components)
		{
			if (comp_node->done != COMPONENT_STATE_DONE) {
				continue;
			}
			DepsNodeFactory *factory = deg_type_get_factory(comp_node->type);
			BLI_assert(factory != NULL);
			id_cow->recalc |= factory->id_recalc_tag();
		}
		GHASH_FOREACH_END();
		DEG_DEBUG_PRINTF("Accumulated recalc bits for %s: %u\n",
		                 id_orig->name, (unsigned int)id_cow->recalc);
		/* Inform editors. */
		if (deg_copy_on_write_is_expanded(id_cow)) {
			deg_editors_id_update(update_ctx, id_cow);
		}
	}
}

}  // namespace

/* Flush updates from tagged nodes outwards until all affected nodes
 * are tagged.
 */
void deg_graph_flush_updates(Main *bmain, Depsgraph *graph)
{
	const bool use_copy_on_write = DEG_depsgraph_use_copy_on_write();
	/* Sanity checks. */
	BLI_assert(bmain != NULL);
	BLI_assert(graph != NULL);
	/* Nothing to update, early out. */
	if (BLI_gset_size(graph->entry_tags) == 0) {
		return;
	}
	/* Reset all flags, get ready for the flush. */
	flush_prepare(graph);
	/* Starting from the tagged "entry" nodes, flush outwards. */
	FlushQueue queue;
	flush_schedule_entrypoints(graph, &queue);
	/* Prepare update context for editors. */
	DEGEditorUpdateContext update_ctx;
	update_ctx.bmain = bmain;
	update_ctx.scene = graph->scene;
	update_ctx.view_layer = graph->view_layer;
	/* Do actual flush. */
	while (!queue.empty()) {
		OperationDepsNode *op_node = queue.front();
		queue.pop_front();
		while (op_node != NULL) {
			/* Tag operation as required for update. */
			op_node->flag |= DEPSOP_FLAG_NEEDS_UPDATE;
			/* Inform corresponding ID and component nodes about the change. */
			ComponentDepsNode *comp_node = op_node->owner;
			IDDepsNode *id_node = comp_node->owner;
			flush_handle_id_node(id_node);
			flush_handle_component_node(graph,
			                            id_node,
			                            comp_node,
			                            use_copy_on_write,
			                            &queue);
			/* Flush to nodes along links. */
			op_node = flush_schedule_children(op_node, &queue);
		}
	}
	/* Inform editors about all changes. */
	flush_editors_id_update(bmain, graph, &update_ctx);
}

static void graph_clear_func(
        void *__restrict data_v,
        const int i,
        const ParallelRangeTLS *__restrict /*tls*/)
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
	ParallelRangeSettings settings;
	BLI_parallel_range_settings_defaults(&settings);
	settings.min_iter_per_thread = 1024;
	BLI_task_parallel_range(0, num_operations,
	                        graph,
	                        graph_clear_func,
	                        &settings);
	/* Clear any entry tags which haven't been flushed. */
	BLI_gset_clear(graph->entry_tags, NULL);
}

}  // namespace DEG
