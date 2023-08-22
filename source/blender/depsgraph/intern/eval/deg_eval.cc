/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Evaluation engine entry-points for Depsgraph Engine.
 */

#include "intern/eval/deg_eval.h"

#include "PIL_time.h"

#include "BLI_compiler_attrs.h"
#include "BLI_function_ref.hh"
#include "BLI_gsqueue.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#include "atomic_ops.h"

#include "intern/depsgraph.h"
#include "intern/depsgraph_relation.h"
#include "intern/depsgraph_tag.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/eval/deg_eval_flush.h"
#include "intern/eval/deg_eval_stats.h"
#include "intern/eval/deg_eval_visibility.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

namespace blender::deg {

namespace {

struct DepsgraphEvalState;

void deg_task_run_func(TaskPool *pool, void *taskdata);

void schedule_children(DepsgraphEvalState *state,
                       OperationNode *node,
                       FunctionRef<void(OperationNode *node)> schedule_fn);

/* Denotes which part of dependency graph is being evaluated. */
enum class EvaluationStage {
  /* Stage 1: Only  Copy-on-Write operations are to be evaluated, prior to anything else.
   * This allows other operations to access its dependencies when there is a dependency cycle
   * involved. */
  COPY_ON_WRITE,

  /* Evaluate actual ID nodes visibility based on the current state of animation and drivers. */
  DYNAMIC_VISIBILITY,

  /* Threaded evaluation of all possible operations. */
  THREADED_EVALUATION,

  /* Workaround for areas which can not be evaluated in threads.
   *
   * For example, meta-balls, which are iterating over all bases and are requesting dupli-lists
   * to see whether there are meta-balls inside. */
  SINGLE_THREADED_WORKAROUND,
};

struct DepsgraphEvalState {
  Depsgraph *graph;
  bool do_stats;
  EvaluationStage stage;
  bool need_update_pending_parents = true;
  bool need_single_thread_pass = false;
};

void evaluate_node(const DepsgraphEvalState *state, OperationNode *operation_node)
{
  ::Depsgraph *depsgraph = reinterpret_cast<::Depsgraph *>(state->graph);

  /* Sanity checks. */
  BLI_assert_msg(!operation_node->is_noop(), "NOOP nodes should not actually be scheduled");
  /* Perform operation. */
  if (state->do_stats) {
    const double start_time = PIL_check_seconds_timer();
    operation_node->evaluate(depsgraph);
    operation_node->stats.current_time += PIL_check_seconds_timer() - start_time;
  }
  else {
    operation_node->evaluate(depsgraph);
  }

  /* Clear the flag early on, allowing partial updates without re-evaluating the same node multiple
   * times.
   * This is a thread-safe modification as the node's flags are only read for a non-scheduled nodes
   * and this node has been scheduled. */
  operation_node->flag &= ~DEPSOP_FLAG_CLEAR_ON_EVAL;
}

void deg_task_run_func(TaskPool *pool, void *taskdata)
{
  void *userdata_v = BLI_task_pool_user_data(pool);
  DepsgraphEvalState *state = (DepsgraphEvalState *)userdata_v;

  /* Evaluate node. */
  OperationNode *operation_node = reinterpret_cast<OperationNode *>(taskdata);
  evaluate_node(state, operation_node);

  /* Schedule children. */
  schedule_children(state, operation_node, [&](OperationNode *node) {
    BLI_task_pool_push(pool, deg_task_run_func, node, false, nullptr);
  });
}

bool check_operation_node_visible(const DepsgraphEvalState *state, OperationNode *op_node)
{
  const ComponentNode *comp_node = op_node->owner;
  /* Special case for copy on write component: it is to be always evaluated, to keep copied
   * "database" in a consistent state. */
  if (comp_node->type == NodeType::COPY_ON_WRITE) {
    return true;
  }

  /* Special case for dynamic visibility pass: the actual visibility is not yet known, so limit to
   * only operations which affects visibility. */
  if (state->stage == EvaluationStage::DYNAMIC_VISIBILITY) {
    return op_node->flag & OperationFlag::DEPSOP_FLAG_AFFECTS_VISIBILITY;
  }

  return comp_node->affects_visible_id;
}

void calculate_pending_parents_for_node(const DepsgraphEvalState *state, OperationNode *node)
{
  /* Update counters, applies for both visible and invisible IDs. */
  node->num_links_pending = 0;
  node->scheduled = false;
  /* Invisible IDs requires no pending operations. */
  if (!check_operation_node_visible(state, node)) {
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
      if (!check_operation_node_visible(state, from)) {
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

void calculate_pending_parents_if_needed(DepsgraphEvalState *state)
{
  if (!state->need_update_pending_parents) {
    return;
  }

  for (OperationNode *node : state->graph->operations) {
    calculate_pending_parents_for_node(state, node);
  }

  state->need_update_pending_parents = false;
}

void initialize_execution(DepsgraphEvalState *state, Depsgraph *graph)
{
  /* Clear tags and other things which needs to be clear. */
  if (state->do_stats) {
    for (OperationNode *node : graph->operations) {
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

    case EvaluationStage::DYNAMIC_VISIBILITY:
      return operation_node->flag & OperationFlag::DEPSOP_FLAG_AFFECTS_VISIBILITY;

    case EvaluationStage::THREADED_EVALUATION:
      if (is_metaball_object_operation(operation_node)) {
        state->need_single_thread_pass = true;
        return false;
      }
      return true;

    case EvaluationStage::SINGLE_THREADED_WORKAROUND:
      return true;
  }
  BLI_assert_msg(0, "Unhandled evaluation stage, should never happen.");
  return false;
}

/* Schedule a node if it needs evaluation.
 *   dec_parents: Decrement pending parents count, true when child nodes are
 *                scheduled after a task has been completed.
 */
void schedule_node(DepsgraphEvalState *state,
                   OperationNode *node,
                   bool dec_parents,
                   const FunctionRef<void(OperationNode *node)> schedule_fn)
{
  /* No need to schedule nodes of invisible ID. */
  if (!check_operation_node_visible(state, node)) {
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
  bool is_scheduled = atomic_fetch_and_or_uint8((uint8_t *)&node->scheduled, uint8_t(true));
  if (!is_scheduled) {
    if (node->is_noop()) {
      /* Clear flags to avoid affecting subsequent update propagation.
       * For normal nodes these are cleared when it is evaluated. */
      node->flag &= ~DEPSOP_FLAG_CLEAR_ON_EVAL;

      /* skip NOOP node, schedule children right away */
      schedule_children(state, node, schedule_fn);
    }
    else {
      /* children are scheduled once this task is completed */
      schedule_fn(node);
    }
  }
}

void schedule_graph(DepsgraphEvalState *state,
                    const FunctionRef<void(OperationNode *node)> schedule_fn)
{
  for (OperationNode *node : state->graph->operations) {
    schedule_node(state, node, false, schedule_fn);
  }
}

void schedule_children(DepsgraphEvalState *state,
                       OperationNode *node,
                       const FunctionRef<void(OperationNode *node)> schedule_fn)
{
  for (Relation *rel : node->outlinks) {
    OperationNode *child = (OperationNode *)rel->to;
    BLI_assert(child->type == NodeType::OPERATION);
    if (child->scheduled) {
      /* Happens when having cyclic dependencies. */
      continue;
    }
    schedule_node(state, child, (rel->flag & RELATION_FLAG_CYCLIC) == 0, schedule_fn);
  }
}

/* Evaluate given stage of the dependency graph evaluation using multiple threads.
 *
 * NOTE: Will assign the `state->stage` to the given stage. */
void evaluate_graph_threaded_stage(DepsgraphEvalState *state,
                                   TaskPool *task_pool,
                                   const EvaluationStage stage)
{
  state->stage = stage;

  calculate_pending_parents_if_needed(state);

  schedule_graph(state, [&](OperationNode *node) {
    BLI_task_pool_push(task_pool, deg_task_run_func, node, false, nullptr);
  });
  BLI_task_pool_work_and_wait(task_pool);
}

/* Evaluate remaining operations of the dependency graph in a single threaded manner. */
void evaluate_graph_single_threaded_if_needed(DepsgraphEvalState *state)
{
  if (!state->need_single_thread_pass) {
    return;
  }

  BLI_assert(!state->need_update_pending_parents);

  state->stage = EvaluationStage::SINGLE_THREADED_WORKAROUND;

  GSQueue *evaluation_queue = BLI_gsqueue_new(sizeof(OperationNode *));
  auto schedule_node_to_queue = [&](OperationNode *node) {
    BLI_gsqueue_push(evaluation_queue, &node);
  };
  schedule_graph(state, schedule_node_to_queue);

  while (!BLI_gsqueue_is_empty(evaluation_queue)) {
    OperationNode *operation_node;
    BLI_gsqueue_pop(evaluation_queue, &operation_node);

    evaluate_node(state, operation_node);
    schedule_children(state, operation_node, schedule_node_to_queue);
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
      (scene_cow->id.recalc & ID_RECALC_COPY_ON_WRITE) == 0)
  {
    return;
  }

  const IDNode *scene_id_node = graph->find_id_node(&graph->scene->id);
  deg_update_copy_on_write_datablock(graph, scene_id_node);
}

TaskPool *deg_evaluate_task_pool_create(DepsgraphEvalState *state)
{
  if (G.debug & G_DEBUG_DEPSGRAPH_NO_THREADS) {
    return BLI_task_pool_create_no_threads(state);
  }

  return BLI_task_pool_create_suspended(state, TASK_PRIORITY_HIGH);
}

}  // namespace

void deg_evaluate_on_refresh(Depsgraph *graph)
{
  /* Nothing to update, early out. */
  if (graph->entry_tags.is_empty()) {
    return;
  }

  graph->debug.begin_graph_evaluation();

#ifdef WITH_PYTHON
  /* Release the GIL so that Python drivers can be evaluated. See #91046. */
  BPy_BEGIN_ALLOW_THREADS;
#endif

  graph->is_evaluating = true;
  depsgraph_ensure_view_layer(graph);

  /* Set up evaluation state. */
  DepsgraphEvalState state;
  state.graph = graph;
  state.do_stats = graph->debug.do_time_debug();

  /* Prepare all nodes for evaluation. */
  initialize_execution(&state, graph);

  /* Evaluation happens in several incremental steps:
   *
   * - Start with the copy-on-write operations which never form dependency cycles. This will ensure
   *   that if a dependency graph has a cycle evaluation functions will always "see" valid expanded
   *   datablock. It might not be evaluated yet, but at least the datablock will be valid.
   *
   * - If there is potentially dynamically changing visibility in the graph update the actual
   *   nodes visibilities, so that actual heavy data evaluation can benefit from knowledge that
   *   something heavy is not currently visible.
   *
   * - Multi-threaded evaluation of all possible nodes.
   *   Certain operations (and their subtrees) could be ignored. For example, meta-balls are not
   *   safe from threading point of view, so the threaded evaluation will stop at the metaball
   *   operation node.
   *
   * - Single-threaded pass of all remaining operations. */

  TaskPool *task_pool = deg_evaluate_task_pool_create(&state);

  evaluate_graph_threaded_stage(&state, task_pool, EvaluationStage::COPY_ON_WRITE);

  if (graph->has_animated_visibility || graph->need_update_nodes_visibility) {
    /* Update pending parents including only the ones which are affecting operations which are
     * affecting visibility. */
    state.need_update_pending_parents = true;

    evaluate_graph_threaded_stage(&state, task_pool, EvaluationStage::DYNAMIC_VISIBILITY);

    deg_graph_flush_visibility_flags_if_needed(graph);

    /* Update parents to an updated visibility and evaluation stage.
     *
     * Need to do it regardless of whether visibility is actually changed or not: current state of
     * the pending parents are all zeroes because it was previously calculated for only visibility
     * related nodes and those are fully evaluated by now. */
    state.need_update_pending_parents = true;
  }

  evaluate_graph_threaded_stage(&state, task_pool, EvaluationStage::THREADED_EVALUATION);

  BLI_task_pool_free(task_pool);

  evaluate_graph_single_threaded_if_needed(&state);

  /* Finalize statistics gathering. This is because we only gather single
   * operation timing here, without aggregating anything to avoid any extra
   * synchronization. */
  if (state.do_stats) {
    deg_eval_stats_aggregate(graph);
  }

  /* Clear any uncleared tags. */
  deg_graph_clear_tags(graph);
  graph->is_evaluating = false;

#ifdef WITH_PYTHON
  BPy_END_ALLOW_THREADS;
#endif

  graph->debug.end_graph_evaluation();
}

}  // namespace blender::deg
