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

/** \file blender/depsgraph/intern/depsgraph_eval.cc
 *  \ingroup depsgraph
 *
 * Evaluation engine entrypoints for Depsgraph Engine.
 */

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_task.h"

#include "BKE_depsgraph.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
} /* extern "C" */

#include "atomic_ops.h"

#include "depsgraph.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_debug.h"

#ifdef WITH_LEGACY_DEPSGRAPH
static bool use_legacy_depsgraph = true;
#endif

bool DEG_depsgraph_use_legacy(void)
{
#ifdef DISABLE_NEW_DEPSGRAPH
	return true;
#elif defined(WITH_LEGACY_DEPSGRAPH)
	return use_legacy_depsgraph;
#else
	BLI_assert(!"Should not be used with new depsgraph");
	return false;
#endif
}

void DEG_depsgraph_switch_to_legacy(void)
{
#ifdef WITH_LEGACY_DEPSGRAPH
	use_legacy_depsgraph = true;
#else
	BLI_assert(!"Should not be used with new depsgraph");
#endif
}

void DEG_depsgraph_switch_to_new(void)
{
#ifdef WITH_LEGACY_DEPSGRAPH
	use_legacy_depsgraph = false;
#else
	BLI_assert(!"Should not be used with new depsgraph");
#endif
}

/* ****************** */
/* Evaluation Context */

/* Create new evaluation context. */
EvaluationContext *DEG_evaluation_context_new(int mode)
{
	EvaluationContext *eval_ctx =
		(EvaluationContext *)MEM_callocN(sizeof(EvaluationContext),
		                                 "EvaluationContext");
	eval_ctx->mode = mode;
	return eval_ctx;
}

/**
 * Initialize evaluation context.
 * Used by the areas which currently overrides the context or doesn't have
 * access to a proper one.
 */
void DEG_evaluation_context_init(EvaluationContext *eval_ctx, int mode)
{
	eval_ctx->mode = mode;
}

/* Free evaluation context. */
void DEG_evaluation_context_free(EvaluationContext *eval_ctx)
{
	MEM_freeN(eval_ctx);
}

/* ********************** */
/* Evaluation Entrypoints */

/* Forward declarations. */
static void schedule_children(TaskPool *pool,
                              Depsgraph *graph,
                              OperationDepsNode *node,
                              const int layers);

struct DepsgraphEvalState {
	EvaluationContext *eval_ctx;
	Depsgraph *graph;
	int layers;
};

static void deg_task_run_func(TaskPool *pool,
                              void *taskdata,
                              int UNUSED(threadid))
{
	DepsgraphEvalState *state = (DepsgraphEvalState *)BLI_task_pool_userdata(pool);
	OperationDepsNode *node = (OperationDepsNode *)taskdata;

	BLI_assert(!node->is_noop() && "NOOP nodes should not actually be scheduled");

	/* Get context. */
	// TODO: who initialises this? "Init" operations aren't able to initialise it!!!
	/* TODO(sergey): Wedon't use component contexts at this moment. */
	/* ComponentDepsNode *comp = node->owner; */
	BLI_assert(node->owner != NULL);
	
	/* Take note of current time. */
	double start_time = PIL_check_seconds_timer();
	DepsgraphDebug::task_started(state->graph, node);
	
	/* Should only be the case for NOOPs, which never get to this point. */
	BLI_assert(node->evaluate);
	
	/* Perform operation. */
	node->evaluate(state->eval_ctx);
	
	/* Note how long this took. */
	double end_time = PIL_check_seconds_timer();
	DepsgraphDebug::task_completed(state->graph,
	                               node,
	                               end_time - start_time);

	schedule_children(pool, state->graph, node, state->layers);
}

static void calculate_pending_parents(Depsgraph *graph, int layers)
{
	for (Depsgraph::OperationNodes::const_iterator it_op = graph->operations.begin();
	     it_op != graph->operations.end();
	     ++it_op)
	{
		OperationDepsNode *node = *it_op;
		IDDepsNode *id_node = node->owner->owner;

		node->num_links_pending = 0;
		node->scheduled = false;

		/* count number of inputs that need updates */
		if ((id_node->layers & layers) != 0 &&
		    (node->flag & DEPSOP_FLAG_NEEDS_UPDATE) != 0)
		{
			for (OperationDepsNode::Relations::const_iterator it_rel = node->inlinks.begin();
			     it_rel != node->inlinks.end();
			     ++it_rel)
			{
				DepsRelation *rel = *it_rel;
				if (rel->from->type == DEPSNODE_TYPE_OPERATION &&
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
}

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

		for (OperationDepsNode::Relations::const_iterator it = node->outlinks.begin();
		     it != node->outlinks.end();
		     ++it)
		{
			DepsRelation *rel = *it;
			OperationDepsNode *to = (OperationDepsNode *)rel->to;
			BLI_assert(to->type == DEPSNODE_TYPE_OPERATION);
			calculate_eval_priority(to);
			node->eval_priority += to->eval_priority;
		}
	}
	else {
		node->eval_priority = 0.0f;
	}
}

/* Schedule a node if it needs evaluation.
 *   dec_parents: Decrement pending parents count, true when child nodes are scheduled
 *                after a task has been completed.
 */
static void schedule_node(TaskPool *pool, Depsgraph *graph, int layers,
                          OperationDepsNode *node, bool dec_parents)
{
	int id_layers = node->owner->owner->layers;
	
	if ((node->flag & DEPSOP_FLAG_NEEDS_UPDATE) != 0 &&
	    (id_layers & layers) != 0)
	{
		if (dec_parents) {
			BLI_assert(node->num_links_pending > 0);
			atomic_sub_uint32(&node->num_links_pending, 1);
		}

		if (node->num_links_pending == 0) {
			bool is_scheduled = atomic_fetch_and_or_uint8((uint8_t*)&node->scheduled, (uint8_t)true);
			if (!is_scheduled) {
				if (node->is_noop()) {
					/* skip NOOP node, schedule children right away */
					schedule_children(pool, graph, node, layers);
				}
				else {
					/* children are scheduled once this task is completed */
					BLI_task_pool_push(pool, deg_task_run_func, node, false, TASK_PRIORITY_LOW);
				}
			}
		}
	}
}

static void schedule_graph(TaskPool *pool,
                           Depsgraph *graph,
                           const int layers)
{
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin();
	     it != graph->operations.end();
	     ++it)
	{
		OperationDepsNode *node = *it;
		schedule_node(pool, graph, layers, node, false);
	}
}

static void schedule_children(TaskPool *pool,
                              Depsgraph *graph,
                              OperationDepsNode *node,
                              const int layers)
{
	for (OperationDepsNode::Relations::const_iterator it = node->outlinks.begin();
	     it != node->outlinks.end();
	     ++it)
	{
		DepsRelation *rel = *it;
		OperationDepsNode *child = (OperationDepsNode *)rel->to;
		BLI_assert(child->type == DEPSNODE_TYPE_OPERATION);

		if (child->scheduled) {
			/* Happens when having cyclic dependencies. */
			continue;
		}

		schedule_node(pool, graph, layers, child, (rel->flag & DEPSREL_FLAG_CYCLIC) == 0);
	}
}

/**
 * Evaluate all nodes tagged for updating,
 * \warning This is usually done as part of main loop, but may also be
 * called from frame-change update.
 *
 * \note Time sources should be all valid!
 */
void DEG_evaluate_on_refresh_ex(EvaluationContext *eval_ctx,
                                Depsgraph *graph,
                                const int layers)
{
	/* Generate base evaluation context, upon which all the others are derived. */
	// TODO: this needs both main and scene access...

	/* Nothing to update, early out. */
	if (graph->entry_tags.size() == 0) {
		return;
	}

	/* Set time for the current graph evaluation context. */
	TimeSourceDepsNode *time_src = graph->find_time_source();
	eval_ctx->ctime = time_src->cfra;

	/* XXX could use a separate pool for each eval context */
	DepsgraphEvalState state;
	state.eval_ctx = eval_ctx;
	state.graph = graph;
	state.layers = layers;

	TaskScheduler *task_scheduler = BLI_task_scheduler_get();
	TaskPool *task_pool = BLI_task_pool_create(task_scheduler, &state);

	if (G.debug & G_DEBUG_DEPSGRAPH_NO_THREADS) {
		BLI_pool_set_num_threads(task_pool, 1);
	}

	calculate_pending_parents(graph, layers);

	/* Clear tags. */
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin();
	     it != graph->operations.end();
	     ++it)
	{
		OperationDepsNode *node = *it;
		node->done = 0;
	}

	/* Calculate priority for operation nodes. */
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin();
	     it != graph->operations.end();
	     ++it)
	{
		OperationDepsNode *node = *it;
		calculate_eval_priority(node);
	}

	DepsgraphDebug::eval_begin(eval_ctx);

	schedule_graph(task_pool, graph, layers);

	BLI_task_pool_work_and_wait(task_pool);
	BLI_task_pool_free(task_pool);

	DepsgraphDebug::eval_end(eval_ctx);

	/* Clear any uncleared tags - just in case. */
	DEG_graph_clear_tags(graph);
}

/* Evaluate all nodes tagged for updating. */
void DEG_evaluate_on_refresh(EvaluationContext *eval_ctx,
                             Depsgraph *graph,
                             Scene *scene)
{
	/* Update time on primary timesource. */
	TimeSourceDepsNode *tsrc = graph->find_time_source();
	tsrc->cfra = BKE_scene_frame_get(scene);

	DEG_evaluate_on_refresh_ex(eval_ctx, graph, graph->layers);
}

/* Frame-change happened for root scene that graph belongs to. */
void DEG_evaluate_on_framechange(EvaluationContext *eval_ctx,
                                 Main *bmain,
                                 Depsgraph *graph,
                                 float ctime,
                                 const int layers)
{
	/* Update time on primary timesource. */
	TimeSourceDepsNode *tsrc = graph->find_time_source();
	tsrc->cfra = ctime;

	tsrc->tag_update(graph);

	DEG_graph_flush_updates(bmain, graph);

	/* Perform recalculation updates. */
	DEG_evaluate_on_refresh_ex(eval_ctx, graph, layers);
}

bool DEG_needs_eval(Depsgraph *graph)
{
	return graph->entry_tags.size() != 0;
}
