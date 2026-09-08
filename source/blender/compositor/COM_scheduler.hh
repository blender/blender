/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_set.hh"
#include "BLI_vector_set.hh"

namespace blender {
struct bNodeTree;
struct bNode;
struct bNodeSocket;
struct ComputeContextHash;
class ComputeContext;
}  // namespace blender

namespace blender::compositor {

class Context;
class Result;

struct Schedule {
  VectorSet<const bNode *> nodes;
  /* Holds the set of all inputs sockets that needn't be computed because the node does not need
   * them, for instance, the unneeded inputs of a Switch node. */
  Set<const bNodeSocket *> unneeded_inputs;
};

/* A function that returns the result associated with the given socket if it is known statically
 * and nullptr otherwise. For instance, the results associated with the sockets of the Group Input
 * and Group Output nodes are known statically because they are those of the node group operation
 * itself, not a node that is yet to be compiled and evaluated. */
using SocketResultFn = FunctionRef<Result *(const bNodeSocket &)>;

/* Computes the execution schedule of the given node group in the given compute context. If the
 * results associated with some of the sockets are known, the socket_result_fn callback should
 * provide them. Only output types and node group outputs that are needed are computed. This is
 * essentially a post-order depth first traversal of the node tree from the needed output nodes to
 * the leaf input nodes, with informed order of traversal of dependencies based on a heuristic
 * estimation of the number of needed buffers. */
Schedule compute_schedule(const Context &context,
                          const bNodeTree &node_group,
                          const ComputeContext &compute_context,
                          SocketResultFn socket_result_fn);

/* Checks if the given node group with the given compute context has an active Viewer node in it or
 * in one of its descendants. Only nodes of node groups whose compute context match that of the
 * given active compute context hash are considered active. */
bool has_viewer_node(const bNodeTree &node_group,
                     const ComputeContext &compute_context,
                     const ComputeContextHash &active_compute_context_hash);

}  // namespace blender::compositor
