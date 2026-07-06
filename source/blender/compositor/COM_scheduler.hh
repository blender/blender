/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_set.hh"
#include "BLI_vector_set.hh"

#include "COM_node_group_operation.hh"

namespace blender::compositor {

struct Schedule {
  VectorSet<const bNode *> nodes;
  /* Holds the set of all inputs sockets that needn't be computed because the node does not need
   * them, for instance, the unneeded inputs of a Switch node. */
  Set<const bNodeSocket *> unneeded_inputs;
};

/* Computes the execution schedule of the given node group operation. Only outputs types and node
 * group outputs that are need are computed. This is essentially a post-order depth first traversal
 * of the node tree from the needed output nodes to the leaf input nodes, with informed order of
 * traversal of dependencies based on a heuristic estimation of the number of needed buffers. */
Schedule compute_schedule(NodeGroupOperation &node_group_operation);

/* Checks if the given node group with the given compute context has an active Viewer node in it or
 * in one of its descendants. Only nodes of node groups whose compute context match that of the
 * given active compute context hash are considered active. */
bool has_viewer_node(const bNodeTree &node_group,
                     const ComputeContext &compute_context,
                     const ComputeContextHash &active_compute_context_hash);

}  // namespace blender::compositor
