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
#include "BLI_gsqueue.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "atomic_ops.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_relation.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/eval/deg_eval_flush.h"
#include "intern/eval/deg_eval_stats.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

namespace blender {
namespace deg {

namespace {

struct DepsgraphEvalState;

void deg_task_run_func(TaskPool *pool, void *taskdata);

template<typename ScheduleFunction, typename... ScheduleFunctionArgs>
void schedule_children(DepsgraphEvalState *state,
                       OperationNode *node,
                       ScheduleFunction *schedule_function,
                       ScheduleFunctionArgs... schedule_function_args);

void schedule_node_to_pool(OperationNode *node, const int UNUSED(thread_id), TaskPool *pool)
{
  BLI_task_pool_push(pool, deg_task_run_func, node, false, NULL);
}

/* Denotes which part of dependency graph is being evaluated. */
enum class EvaluationStage {
  /* Stage 1: Only  Copy-on-Write operations are to be evaluated, prior to anything else.
   * This allows other operations to access its dependencies when there is a dependency cycle
   * involved. */
  COPY_ON_WRITE,

  /* Threaded evaluation of all possible operations. */
  THREADED_EVALUATION,

  /* Workaround for areas which can not be evaluated in threads.
   *
   * For example, metaballs, which are iterating over all bases and are requesting dupli-lists
   * to see whether there are metaballs inside. */
  SINGLE_THREADED_WORKAROUND,
};

struct DepsgraphEvalState {
  Depsgraph *graph;
  bool do_stats;
  EvaluationStage stage;
  bool need_single_thread_pass;
};

void evaluate_node(const DepsgraphEvalState *state, OperationNode *operation_node)
{
  ::Depsgraph *depsgraph = reinterpret_cast<::Depsgraph *>(state->graph);

  /* Sanity checks. */
  BLI_assert(!operation_node->is_noop() && "NOOP nodes should not actually be scheduled");
  /* Perform operation. */
  if (state->do_stats) {
    const double start_time = PIL_check_seconds_timer();
    operation_node->evaluate(depsgraph);
    operation_node->stats.current_time += PIL_check_seconds_timer() - start_time;
  }
  else {
    operation_node->evaluate(depsgraph);
  }
}

void deg_task_run_func(TaskPool *pool, void *taskdata)
{
  void *userdata_v = BLI_task_pool_user_data(pool);
  DepsgraphEvalState *state = (DepsgraphEvalState *)userdata_v;

  /* Evaluate node. */
  OperationNode *operation_node = reinterpret_cast<OperationNode *>(taskdata);
  evaluate_node(state, operation_node);

  /* Schedule children. */
  schedule_children(state, operation_node, schedule_node_to_pool, pool);
}

bool check_operation_node_visible(OperationNode *op_node)
{
  const ComponentNode *comp_node = op_node->owner;
  /* Special exception, copy on write component is to be always evaluated,
   * to keep copied "database" in a consistent state. */
  if (comp_node->type == NodeType::COPY_ON_WRITE) {
    return true;
  }
  return comp_node->affects_directly_visible;
}

void calculate_pending_parents_for_node(OperationNode *node)
{
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
      /* No need to wait for operation which is up to date. */
      if ((from->flag & DEPSOP_FLAG_NEEDS_UPDATE) == 0) {
        continue;
      }
      ++node->num_links_pending;
    }
  }
}

void calculate_pending_parents(Depsgraph *graph)
{
  for (OperationNode *node : graph->operations) {
    calculate_pending_parents_for_node(node);
  }
}

void initialize_execution(DepsgraphEvalState *state, Depsgraph *graph)
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

bool is_metaball_object_operation(const OperationNode *operation_node)
{
  const ComponentNode *component_node = operation_node->owner;
  const IDNode *id_node = component_node->owner;
  if (GS(id_node->id_cow->name) != ID_OB) {
    return false;
  }
  const Object *object = reinterpret_cast<const Object *>(id_node->id_cow);
  return object->type == OB_MBALL;
}

bool need_evaluate_operation_at_stage(DepsgraphEvalState *state,
                                      const OperationNode *operation_node)
{
  const ComponentNode *component_node = operation_node->owner;
  switch (state->stage) {
    case EvaluationStage::COPY_ON_WRITE:
      return (component_node->type == NodeType::COPY_ON_WRITE);

    case EvaluationStage::THREADED_EVALUATION:
      /* Sanity check: copy-on-write node should be evaluated already. This will be indicated by
       * scheduled flag (we assume that scheduled operations have been actually handled by previous
       * stage). */
      BLI_assert(operation_node->scheduled || component_node->type != NodeType::COPY_ON_WRITE);
      if (is_metaball_object_operation(operation_node)) {
        state->need_single_thread_pass = true;
        return false;
      }
      return true;

    case EvaluationStage::SINGLE_THREADED_WORKAROUND:
      return true;
  }
  BLI_assert(!"Unhandled evaluation stage, should never happen.");
  return false;
}

/* Schedule a node if it needs evaluation.
 *   dec_parents: Decrement pending parents count, true when child nodes are
 *                scheduled after a task has been completed.
 */
template<typename ScheduleFunction, typename... ScheduleFunctionArgs>
void schedule_node(DepsgraphEvalState *state,
                   OperationNode *node,
                   bool dec_parents,
                   ScheduleFunction *schedule_function,
                   ScheduleFunctionArgs... schedule_function_args)
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
  if (!need_evaluate_operation_at_stage(state, node)) {
    return;
  }
  /* Actually schedule the node. */
  bool is_scheduled = atomic_fetch_and_or_uint8((uint8_t *)&node->scheduled, (uint8_t) true);
  if (!is_scheduled) {
    if (node->is_noop()) {
      /* skip NOOP node, schedule children right away */
      schedule_children(state, node, schedule_function, schedule_function_args...);
    }
    else {
      /* children are scheduled once this task is completed */
      schedule_function(node, 0, schedule_function_args...);
    }
  }
}

template<typename ScheduleFunction, typename... ScheduleFunctionArgs>
void schedule_graph(DepsgraphEvalState *state,
                    ScheduleFunction *schedule_function,
                    ScheduleFunctionArgs... schedule_function_args)
{
  for (OperationNode *node : state->graph->operations) {
    schedule_node(state, node, false, schedule_function, schedule_function_args...);
  }
}

template<typename ScheduleFunction, typename... ScheduleFunctionArgs>
void schedule_children(DepsgraphEvalState *state,
                       OperationNode *node,
                       ScheduleFunction *schedule_function,
                       ScheduleFunctionArgs... schedule_function_args)
{
  for (Relation *rel : node->outlinks) {
    OperationNode *child = (OperationNode *)rel->to;
    BLI_assert(child->type == NodeType::OPERATION);
    if (child->scheduled) {
      /* Happens when having cyclic dependencies. */
      continue;
    }
    schedule_node(state,
                  child,
                  (rel->flag & RELATION_FLAG_CYCLIC) == 0,
                  schedule_function,
                  schedule_function_args...);
  }
}

void schedule_node_to_queue(OperationNode *node,
                            const int /*thread_id*/,
                            GSQueue *evaluation_queue)
{
  BLI_gsqueue_push(evaluation_queue, &node);
}

void evaluate_graph_single_threaded(DepsgraphEvalState *state)
{
  GSQueue *evaluation_queue = BLI_gsqueue_new(sizeof(OperationNode *));
  schedule_graph(state, schedule_node_to_queue, evaluation_queue);

  while (!BLI_gsqueue_is_empty(evaluation_queue)) {
    OperationNode *operation_node;
    BLI_gsqueue_pop(evaluation_queue, &operation_node);

    evaluate_node(state, operation_node);
    schedule_children(state, operation_node, schedule_node_to_queue, evaluation_queue);
  }

  BLI_gsqueue_free(evaluation_queue);
}

void depsgraph_ensure_view_layer(Depsgraph *graph)
{
  /* We update copy-on-write scene in the following cases:
   * - It was not expanded yet.
   * - It was tagged for update of CoW component.
   * This allows us to have proper view layer pointer. */
  Scene *scene_cow = graph->scene_cow;
  if (deg_copy_on_write_is_expanded(&scene_cow->id) &&
      (scene_cow->id.recalc & ID_RECALC_COPY_ON_WRITE) == 0) {
    return;
  }

  const IDNode *scene_id_node = graph->find_id_node(&graph->scene->id);
  deg_update_copy_on_write_datablock(graph, scene_id_node);
}

}  // namespace

static TaskPool *deg_evaluate_task_pool_create(DepsgraphEvalState *state)
{
  if (G.debug & G_DEBUG_DEPSGRAPH_NO_THREADS) {
    return BLI_task_pool_create_no_threads(state);
  }

  return BLI_task_pool_create_suspended(state, TASK_PRIORITY_HIGH);
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
  if (graph->entry_tags.is_empty()) {
    return;
  }

  graph->debug.begin_graph_evaluation();

  graph->is_evaluating = true;
  depsgraph_ensure_view_layer(graph);
  /* Set up evaluation state. */
  DepsgraphEvalState state;
  state.graph = graph;
  state.do_stats = graph->debug.do_time_debug();
  state.need_single_thread_pass = false;
  /* Prepare all nodes for evaluation. */
  initialize_execution(&state, graph);

  /* Do actual evaluation now. */
  /* First, process all Copy-On-Write nodes. */
  state.stage = EvaluationStage::COPY_ON_WRITE;
  TaskPool *task_pool = deg_evaluate_task_pool_create(&state);
  schedule_graph(&state, schedule_node_to_pool, task_pool);
  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  /* After that, process all other nodes. */
  state.stage = EvaluationStage::THREADED_EVALUATION;
  task_pool = deg_evaluate_task_pool_create(&state);
  schedule_graph(&state, schedule_node_to_pool, task_pool);
  BLI_task_pool_work_and_wait(task_pool);
  BLI_task_pool_free(task_pool);

  if (state.need_single_thread_pass) {
    state.stage = EvaluationStage::SINGLE_THREADED_WORKAROUND;
    evaluate_graph_single_threaded(&state);
  }

  /* Finalize statistics gathering. This is because we only gather single
   * operation timing here, without aggregating anything to avoid any extra
   * synchronization. */
  if (state.do_stats) {
    deg_eval_stats_aggregate(graph);
  }
  /* Clear any uncleared tags - just in case. */
  deg_graph_clear_tags(graph);
  graph->is_evaluating = false;

  graph->debug.end_graph_evaluation();
}

}  // namespace deg
}  // namespace blender
