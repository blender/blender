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

/** \file blender/depsgraph/intern/eval/deg_eval.cc
 *  \ingroup depsgraph
 *
 * Evaluation engine entrypoints for Depsgraph Engine.
 */

#include "intern/eval/deg_eval.h"

#include "PIL_time.h"

#include "BLI_utildefines.h"
#include "BLI_task.h"
#include "BLI_ghash.h"

extern "C" {
#include "BKE_depsgraph.h"
#include "BKE_global.h"
} /* extern "C" */

#include "DEG_depsgraph.h"

#include "atomic_ops.h"

#include "intern/eval/deg_eval_debug.h"
#include "intern/eval/deg_eval_flush.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_component.h"
#include "intern/nodes/deg_node_operation.h"
#include "intern/depsgraph.h"
#include "intern/depsgraph_intern.h"
#include "util/deg_util_foreach.h"

/* Unfinished and unused, and takes quite some pre-processing time. */
#undef USE_EVAL_PRIORITY

/* Use integrated debugger to keep track how much each of the nodes was
 * evaluating.
 */
#undef USE_DEBUGGER

namespace DEG {

/* ********************** */
/* Evaluation Entrypoints */

/* Forward declarations. */
static void schedule_children(TaskPool *pool,
                              Depsgraph *graph,
                              OperationDepsNode *node,
                              const unsigned int layers,
                              const int thread_id);

struct DepsgraphEvalState {
	EvaluationContext *eval_ctx;
	Depsgraph *graph;
	unsigned int layers;
};

static void deg_task_run_func(TaskPool *pool,
                              void *taskdata,
                              int thread_id)
{
	DepsgraphEvalState *state =
	        reinterpret_cast<DepsgraphEvalState *>(BLI_task_pool_userdata(pool));
	OperationDepsNode *node = reinterpret_cast<OperationDepsNode *>(taskdata);

	BLI_assert(!node->is_noop() && "NOOP nodes should not actually be scheduled");

	/* Should only be the case for NOOPs, which never get to this point. */
	BLI_assert(node->evaluate);

	/* Get context. */
	/* TODO: Who initialises this? "Init" operations aren't able to
	 * initialise it!!!
	 */
	/* TODO(sergey): We don't use component contexts at this moment. */
	/* ComponentDepsNode *comp = node->owner; */
	BLI_assert(node->owner != NULL);

	/* Since we're not leaving the thread for until the graph branches it is
	 * possible to have NO-OP on the way. for which evaluate() will be NULL.
	 * but that's all fine, we'll just scheduler it's children.
	 */
	if (node->evaluate) {
			/* Take note of current time. */
#ifdef USE_DEBUGGER
		double start_time = PIL_check_seconds_timer();
		DepsgraphDebug::task_started(state->graph, node);
#endif

		/* Perform operation. */
		node->evaluate(state->eval_ctx);

			/* Note how long this took. */
#ifdef USE_DEBUGGER
		double end_time = PIL_check_seconds_timer();
		DepsgraphDebug::task_completed(state->graph,
		                               node,
		                               end_time - start_time);
#endif
	}

	BLI_task_pool_delayed_push_begin(pool, thread_id);
	schedule_children(pool, state->graph, node, state->layers, thread_id);
	BLI_task_pool_delayed_push_end(pool, thread_id);
}

typedef struct CalculatePengindData {
	Depsgraph *graph;
	unsigned int layers;
} CalculatePengindData;

static void calculate_pending_func(void *data_v, int i)
{
	CalculatePengindData *data = (CalculatePengindData *)data_v;
	Depsgraph *graph = data->graph;
	unsigned int layers = data->layers;
	OperationDepsNode *node = graph->operations[i];
	IDDepsNode *id_node = node->owner->owner;

	node->num_links_pending = 0;
	node->scheduled = false;

	/* count number of inputs that need updates */
	if ((id_node->layers & layers) != 0 &&
	    (node->flag & DEPSOP_FLAG_NEEDS_UPDATE) != 0)
	{
		foreach (DepsRelation *rel, node->inlinks) {
			if (rel->from->type == DEG_NODE_TYPE_OPERATION &&
			    (rel->flag & DEPSREL_FLAG_CYCLIC) == 0)
			{
				OperationDepsNode *from = (OperationDepsNode *)rel->from;
				IDDepsNode *id_from_node = from->owner->owner;
				if ((id_from_node->layers & layers) != 0 &&
				    (from->flag & DEPSOP_FLAG_NEEDS_UPDATE) != 0)
				{
					++node->num_links_pending;
				}
			}
		}
	}
}

static void calculate_pending_parents(Depsgraph *graph, unsigned int layers)
{
	const int num_operations = graph->operations.size();
	const bool do_threads = num_operations > 256;
	CalculatePengindData data;
	data.graph = graph;
	data.layers = layers;
	BLI_task_parallel_range(0,
	                        num_operations,
	                        &data,
	                        calculate_pending_func,
	                        do_threads);
}

#ifdef USE_EVAL_PRIORITY
static void calculate_eval_priority(OperationDepsNode *node)
{
	if (node->done) {
		return;
	}
	node->done = 1;

	if (node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
		/* XXX standard cost of a node, could be estimated somewhat later on */
		const float cost = 1.0f;
		/* NOOP nodes have no cost */
		node->eval_priority = node->is_noop() ? cost : 0.0f;

		foreach (DepsRelation *rel, node->outlinks) {
			OperationDepsNode *to = (OperationDepsNode *)rel->to;
			BLI_assert(to->type == DEG_NODE_TYPE_OPERATION);
			calculate_eval_priority(to);
			node->eval_priority += to->eval_priority;
		}
	}
	else {
		node->eval_priority = 0.0f;
	}
}
#endif

/* Schedule a node if it needs evaluation.
 *   dec_parents: Decrement pending parents count, true when child nodes are
 *                scheduled after a task has been completed.
 */
static void schedule_node(TaskPool *pool, Depsgraph *graph, unsigned int layers,
                          OperationDepsNode *node, bool dec_parents,
                          const int thread_id)
{
	unsigned int id_layers = node->owner->owner->layers;

	if ((node->flag & DEPSOP_FLAG_NEEDS_UPDATE) != 0 &&
	    (id_layers & layers) != 0)
	{
		if (dec_parents) {
			BLI_assert(node->num_links_pending > 0);
			atomic_sub_and_fetch_uint32(&node->num_links_pending, 1);
		}

		if (node->num_links_pending == 0) {
			bool is_scheduled = atomic_fetch_and_or_uint8(
			        (uint8_t *)&node->scheduled, (uint8_t)true);
			if (!is_scheduled) {
				if (node->is_noop()) {
					/* skip NOOP node, schedule children right away */
					schedule_children(pool, graph, node, layers, thread_id);
				}
				else {
					/* children are scheduled once this task is completed */
					BLI_task_pool_push_from_thread(pool,
					                               deg_task_run_func,
					                               node,
					                               false,
					                               TASK_PRIORITY_HIGH,
					                               thread_id);
				}
			}
		}
	}
}

static void schedule_graph(TaskPool *pool,
                           Depsgraph *graph,
                           const unsigned int layers)
{
	foreach (OperationDepsNode *node, graph->operations) {
		schedule_node(pool, graph, layers, node, false, 0);
	}
}

static void schedule_children(TaskPool *pool,
                              Depsgraph *graph,
                              OperationDepsNode *node,
                              const unsigned int layers,
                              const int thread_id)
{
	foreach (DepsRelation *rel, node->outlinks) {
		OperationDepsNode *child = (OperationDepsNode *)rel->to;
		BLI_assert(child->type == DEG_NODE_TYPE_OPERATION);
		if (child->scheduled) {
			/* Happens when having cyclic dependencies. */
			continue;
		}
		schedule_node(pool,
		              graph,
		              layers,
		              child,
		              (rel->flag & DEPSREL_FLAG_CYCLIC) == 0,
		              thread_id);
	}
}

/**
 * Evaluate all nodes tagged for updating,
 * \warning This is usually done as part of main loop, but may also be
 * called from frame-change update.
 *
 * \note Time sources should be all valid!
 */
void deg_evaluate_on_refresh(EvaluationContext *eval_ctx,
                             Depsgraph *graph,
                             const unsigned int layers)
{
	/* Generate base evaluation context, upon which all the others are derived. */
	// TODO: this needs both main and scene access...

	/* Nothing to update, early out. */
	if (BLI_gset_size(graph->entry_tags) == 0) {
		return;
	}

	DEG_DEBUG_PRINTF("%s: layers:%u, graph->layers:%u\n",
	                 __func__,
	                 layers,
	                 graph->layers);

	/* Set time for the current graph evaluation context. */
	TimeSourceDepsNode *time_src = graph->find_time_source();
	eval_ctx->ctime = time_src->cfra;

	/* XXX could use a separate pool for each eval context */
	DepsgraphEvalState state;
	state.eval_ctx = eval_ctx;
	state.graph = graph;
	state.layers = layers;

	TaskScheduler *task_scheduler;
	bool need_free_scheduler;

	if (G.debug & G_DEBUG_DEPSGRAPH_NO_THREADS) {
		task_scheduler = BLI_task_scheduler_create(1);
		need_free_scheduler = true;
	}
	else {
		task_scheduler = BLI_task_scheduler_get();
		need_free_scheduler = false;
	}

	TaskPool *task_pool = BLI_task_pool_create_suspended(task_scheduler, &state);

	calculate_pending_parents(graph, layers);

	/* Clear tags. */
	foreach (OperationDepsNode *node, graph->operations) {
		node->done = 0;
	}

	/* Calculate priority for operation nodes. */
#ifdef USE_EVAL_PRIORITY
	foreach (OperationDepsNode *node, graph->operations) {
		calculate_eval_priority(node);
	}
#endif

	DepsgraphDebug::eval_begin(eval_ctx);

	schedule_graph(task_pool, graph, layers);

	BLI_task_pool_work_and_wait(task_pool);
	BLI_task_pool_free(task_pool);

	DepsgraphDebug::eval_end(eval_ctx);

	/* Clear any uncleared tags - just in case. */
	deg_graph_clear_tags(graph);

	if (need_free_scheduler) {
		BLI_task_scheduler_free(task_scheduler);
	}
}

}  // namespace DEG
