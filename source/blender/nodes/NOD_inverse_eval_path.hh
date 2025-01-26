/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_compute_context.hh"

#include "NOD_value_elem.hh"

namespace blender::nodes::inverse_eval {

using namespace value_elem;

struct LocalInverseEvalTargets {
  Vector<SocketElem> input_sockets;
  Vector<GroupInputElem> group_inputs;
  Vector<ValueNodeElem> value_nodes;
};

/**
 * Scans the node tree backwards from the given socket to figure out which values may need to
 * change to set the given socket to a specific value.
 */
LocalInverseEvalTargets find_local_inverse_eval_targets(const bNodeTree &tree,
                                                        const SocketElem &initial_socket_elem);

/**
 * Traverses the inverse evaluation path that starts at the given socket in a specific compute
 * context.
 *
 * \param initial_context: Compute context where the inverse evaluation starts (e.g. may be deep in
 *   some nested node group).
 * \param initial_socket_elem: Socket and value element that is propagated backwards.
 * \param foreach_context_fn: If provided, it is called for each compute context that is touched by
 *   the inverse evaluation path.
 * \param foreach_socket_fn: If provided, it is called for each socket on the inverse evaluation
 *   path.
 */
void foreach_element_on_inverse_eval_path(
    const ComputeContext &initial_context,
    const SocketElem &initial_socket_elem,
    FunctionRef<void(const ComputeContext &context)> foreach_context_fn,
    FunctionRef<void(const ComputeContext &context,
                     const bNodeSocket &socket,
                     const ElemVariant &elem)> foreach_socket_fn);

}  // namespace blender::nodes::inverse_eval
