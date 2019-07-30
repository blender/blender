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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Evaluation engine entrypoints for Depsgraph Engine.
 */

#include "intern/eval/deg_eval.h"

#include "PIL_time.h"

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"
#include "BLI_task.h"
#include "BLI_ghash.h"

#include "BKE_global.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "atomic_ops.h"

#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/eval/deg_eval_flush.h"
#include "intern/eval/deg_eval_stats.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"
#include "intern/depsgraph.h"

namespace DEG {

/* ********************** */
/* Evaluation Entrypoints */

/* Forward declarations. */
static void schedule_children(TaskPool *pool,
                              Depsgraph *graph,
                              OperationNode *node,
                              const int thread_id);

struct DepsgraphEvalState {
  Depsgraph *graph;
  bool do_stats;
  bool is_cow_stage;
};

static void deg_task_run_func(TaskPool *pool, void *taskdata, int thread_id)
{
  void *userdata_v = BLI_task_pool_userdata(pool);
  DepsgraphEvalState *state = (DepsgraphEvalState *)userdata_v;
  OperationNode *node = (OperationNode *)taskdata;
  /* Sanity checks. */
  BLI_assert(!node->is_noop() && "NOOP nodes should not actually be scheduled");
  /* Perform operation. */
  if (state->do_stats) {
    const double start_time = PIL_check_seconds_timer();
    node->evaluate((::Depsgraph *)state->graph);
    node->stats.current_time += PIL_check_seconds_timer() - start_time;
  }
  else {
    node->evaluate((::Depsgraph *)state->graph);
  }
  /* Schedule children. */
  BLI_task_pool_delayed_push_begin(pool, thread_id);
  schedule_children(pool, state->graph, node, thread_id);
  BLI_task_pool_delayed_push_end(pool, thread_id);
}

struct CalculatePendingData {
  Depsgraph *graph;
};

static bool check_operation_node_visible(OperationNode *op_node)
{
  const ComponentNode *comp_node = op_node->owner;
  /* Special exception, copy on write component is to be always evaluated,
   * to keep copied "database" in a consistent state. */
  if (comp_node->type == NodeType::COPY_ON_WRITE) {
    return true;
  }
  return comp_node->affects_directly_visible;
}

static void calculate_pending_func(void *__restrict data_v,
                                   const int i,
                                   const TaskParallelTLS *__restrict /*tls*/)
{
  CalculatePendingData *data = (CalculatePendingData *)data_v;
  Depsgraph *graph = data->graph;
  OperationNode *node = graph->operations[i];
  /* Update counters, applies for both visible and invisible IDs. */
  node->num_links_pending = 0;
  node->scheduled = false;
  /* Invisible IDs requires no pending operations. */
  if (!check_operation_node_visible(node)) {
    return;
  }
  /* No need to bother with anything if node is not tagged for update. */
  if ((node->flag & DEPSOP_FLAG_NEEDS_UPDATE) == 0) {
    return;
  }
  for (Relation *rel : node->inlinks) {
    if (rel->from->type == NodeType::OPERATION && (rel->flag & RELATION_FLAG_CYCLIC) == 0) {
      OperationNode *from = (OperationNode *)rel->from;
      /* TODO(sergey): This is how old layer system was checking for the
       * calculation, but how is it possible that visible object depends
       * on an invisible? This is something what is prohibited after
       * deg_graph_build_flush_layers(). */
      if (!check_operation_node_visible(from)) {
        continue;
      }
      /* No need to vait for operation which is up to date. */
      if ((from->flag & DEPSOP_FLAG_NEEDS_UPDATE) == 0) {
        continue;
      }
      ++node->num_links_pending;
    }
  }
}

static void calculate_pending_parents(Depsgraph *graph)
{
  const int num_operations = graph->operations.size();
  CalculatePendingData data;
  data.graph = graph;
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.min_iter_per_thread = 1024;
  BLI_task_parallel_range(0, num_operations, &data, calculate_pending_func, &settings);
}

static void initialize_execution(DepsgraphEvalState *state, Depsgraph *graph)
{
  const bool do_stats = state->do_stats;
  calculate_pending_parents(graph);
  /* Clear tags and other things which needs to be clear. */
  for (OperationNode *node : graph->operations) {
    if (do_stats) {
      node->stats.reset_current();
    }
  }
}

/* Schedule a node if it needs evaluation.
 *   dec_parents: Decrement pending parents count, true when child nodes are
 *                scheduled after a task has been completed.
 */
static void schedule_node(
    TaskPool *pool, Depsgraph *graph, OperationNode *node, bool dec_parents, const int thread_id)
{
  /* No need to schedule nodes of invisible ID. */
  if (!check_operation_node_visible(node)) {
    return;
  }
  /* No need to schedule operations which are not tagged for update, they are
   * considered to be up to date. */
  if ((node->flag & DEPSOP_FLAG_NEEDS_UPDATE) == 0) {
    return;
  }
  /* TODO(sergey): This is not strictly speaking safe to read
   * num_links_pending. */
  if (dec_parents) {
    BLI_assert(node->num_links_pending > 0);
    atomic_sub_and_fetch_uint32(&node->num_links_pending, 1);
  }
  /* Cal not schedule operation while its dependencies are not yet
   * evaluated. */
  if (node->num_links_pending != 0) {
    return;
  }
  /* During the COW stage only schedule COW nodes. */
  DepsgraphEvalState *state = (DepsgraphEvalState *)BLI_task_pool_userdata(pool);
  if (state->is_cow_stage) {
    if (node->owner->type != NodeType::COPY_ON_WRITE) {
      return;
    }
  }
  else {
    BLI_assert(node->scheduled || node->owner->type != NodeType::COPY_ON_WRITE);
  }
  /* Actually schedule the node. */
  bool is_scheduled = atomic_fetch_and_or_uint8((uint8_t *)&node->scheduled, (uint8_t) true);
  if (!is_scheduled) {
    if (node->is_noop()) {
      /* skip NOOP node, schedule children right away */
      schedule_children(pool, graph, node, thread_id);
    }
    else {
      /* children are scheduled once this task is completed */
      BLI_task_pool_push_from_thread(
          pool, deg_task_run_func, node, false, TASK_PRIORITY_HIGH, thread_id);
    }
  }
}

static void schedule_graph(TaskPool *pool, Depsgraph *graph)
{
  for (OperationNode *node : graph->operations) {
    schedule_node(pool, graph, node, false, 0);
  }
}

static void schedule_children(TaskPool *pool,
                              Depsgraph *graph,
                              OperationNode *node,
                              const int thread_id)
{
  for (Relation *rel : node->outlinks) {
    OperationNode *child = (OperationNode *)rel->to;
    BLI_assert(child->type == NodeType::OPERATION);
    if (child->scheduled) {
      /* Happens when having cyclic dependencies. */
      continue;
    }
    schedule_node(pool, graph, child, (rel->flag & RELATION_FLAG_CYCLIC) == 0, thread_id);
  }
}

static void depsgraph_ensure_view_layer(Depsgraph *graph)
{
  /* We update copy-on-write scene in the following cases:
   * - It was not expanded yet.
   * - It was tagged for update of CoW component.
   * This allows us to have proper view layer pointer. */
  Scene *scene_cow = graph->scene_cow;
  if (!deg_copy_on_write_is_expanded(&scene_cow->id) ||
      scene_cow->id.recalc & ID_RECALC_COPY_ON_WRITE) {
    const IDNode *id_node = graph->find_id_node(&graph->scene->id);
    deg_update_copy_on_write_datablock(graph, id_node);
  }
}

/**
 * Evaluate all nodes tagged for updating,
 * \warning This is usually done as part of main loop, but may also be
 * called from frame-change update.
 *
 * \note Time sources should be all valid!
 */
void deg_evaluate_on_refresh(Depsgraph *graph)
{
  /* Nothing to update, early out. */
  if (BLI_gset_len(graph->entry_tags) == 0) {
    return;
  }
  const bool do_time_debug = ((G.debug & G_DEBUG_DEPSGRAPH_TIME) != 0);
  const double start_time = do_time_debug ? PIL_check_seconds_timer() : 0;
  graph->debug_is_evaluating = true;
  depsgraph_ensure_view_layer(graph);
  /* Set up evaluation state. */
  DepsgraphEvalState state;
  state.graph = graph;
  state.do_stats = do_time_debug;
  /* Set up task scheduler and pull for threaded evaluation. */
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
  /* Prepare all nodes for evaluation. */
  initialize_execution(&state, graph);
  /* Do actual evaluation now. */
  /* First, process all Copy-On-Write nodes. */
  state.is_cow_stage = true;
  schedule_graph(task_pool, graph);
  BLI_task_pool_work_wait_and_reset(task_pool);
  /* After that, process all other nodes. */
  state.is_cow_stage = false;
  schedule_graph(task_pool, graph);
  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);
  /* Finalize statistics gathering. This is because we only gather single
   * operation timing here, without aggregating anything to avoid any extra
   * synchronization. */
  if (state.do_stats) {
    deg_eval_stats_aggregate(graph);
  }
  /* Clear any uncleared tags - just in case. */
  deg_graph_clear_tags(graph);
  if (need_free_scheduler) {
    BLI_task_scheduler_free(task_scheduler);
  }
  graph->debug_is_evaluating = false;
  if (do_time_debug) {
    printf("Depsgraph updated in %f seconds.\n", PIL_check_seconds_timer() - start_time);
  }
}

}  // namespace DEG
