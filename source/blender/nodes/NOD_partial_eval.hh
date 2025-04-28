/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_node_in_compute_context.hh"

#include "BLI_function_ref.hh"
#include "BLI_set.hh"

#include "BKE_compute_context_cache.hh"

#include "DNA_node_types.h"

/**
 * This header provides functionality that makes it relatively straight forward to evaluate parts
 * of a node tree. The evaluator is designed to be flexible and simple to use in different
 * contexts. It's not designed to be highly efficient and parallel. However, it has a lower
 * start-up cost compared to e.g. the lazy-function evaluation for geometry nodes, which needs to
 * convert the entire node graph into a lazy-function graph first. So it can be more efficient when
 * only very few nodes of a larger graph have to be evaluated and those nodes are cheap.
 *
 * The evaluator does not use recursion, so it can be used on node graphs of every size and depth.
 */
namespace blender::nodes::partial_eval {

/**
 * Evaluating part of a node tree from left-to-right. The part that's evaluated starts at
 * the given sockets and is propagated downstream step-by-step. The caller is responsible for
 * storing the socket values (a value per #SocketInContext).
 *
 * \note This handles node groups transparently, but does not handle e.g. repeat zones yet.
 *
 * \param initial_sockets: Sockets where the evaluation should start.
 * \param scope: Is used to construct compute contexts which the caller may want to outlive the
 *   entire evaluation.
 * \param evaluate_node_fn: Is called when all (relevant) upstream nodes are already evaluated and
 *   evaluates the given node. This should updated the values the caller stores for the output
 *   sockets.
 * \param propagate_value_fn: Should copy the value stored for one socket to the other socket. This
 *   may have to do type conversions. The return value indicates success. False indicates that the
 *   value was not propagated and as such the target node also shouldn't be evaluated (unless there
 *   are other reasons to evaluate it).
 */
void eval_downstream(
    const Span<SocketInContext> initial_sockets,
    bke::ComputeContextCache &compute_context_cache,
    FunctionRef<void(const NodeInContext &ctx_node,
                     Vector<const bNodeSocket *> &r_outputs_to_propagate)> evaluate_node_fn,
    FunctionRef<bool(const SocketInContext &ctx_from, const SocketInContext &ctx_to)>
        propagate_value_fn);

struct UpstreamEvalTargets {
  Set<SocketInContext> sockets;
  Set<NodeInContext> value_nodes;
  Set<SocketInContext> group_inputs;
};

/**
 * Evaluates part of a node tree from right-to-left (inverse direction). The caller is responsible
 * for storing the socket values (a value per #SocketInContext). Evaluation in the upstream
 * direction is not always well defined, because output sockets may be linked to multiple inputs
 * and nodes may not always have an inverse evaluation function. The caller is responsible for
 * handling these cases gracefully in the given callbacks.
 *
 * \note This handles node groups transparently, but does not handle e.g. repeat zones yet.
 *
 * \param initial_sockets: Sockets where the evaluation should start.
 * \param scope: Is used to construct compute contexts which the caller may want to outlive the
 *   entire evaluation.
 * \param evaluate_node_fn: Called to evaluate the node in reverse, i.e. it's outputs are computed
 *   first, and the node evaluation computes the inputs.
 * \param propagate_value_fn: Should copy the value from one socket to another, while optionally
 *   doing type conversions. This has to handle the case when multiple values are propagated to the
 *   same socket. Returning false indicates that no value was propagated.
 * \param get_inputs_to_propagate_fn: Gathers a list of input sockets that should be propagated
 *   further.
 *
 * \return Places in the node tree that have gotten new values that can't be propagated further in
 *   the node tree.
 */
UpstreamEvalTargets eval_upstream(
    const Span<SocketInContext> initial_sockets,
    bke::ComputeContextCache &compute_context_cache,
    FunctionRef<void(const NodeInContext &ctx_node,
                     Vector<const bNodeSocket *> &r_modified_inputs)> evaluate_node_fn,
    FunctionRef<bool(const SocketInContext &ctx_from, const SocketInContext &ctx_to)>
        propagate_value_fn,
    FunctionRef<void(const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_sockets)>
        get_inputs_to_propagate_fn);

bool is_supported_value_node(const bNode &node);

}  // namespace blender::nodes::partial_eval
