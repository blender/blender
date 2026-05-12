/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_set.hh"
#include "BLI_vector_set.hh"

#include "COM_context.hh"
#include "COM_node_group_operation.hh"

namespace blender::compositor {

struct Schedule {
  VectorSet<const bNode *> nodes;
  /* Holds the set of all inputs sockets that needn't be computed because the node does not need
   * them, for instance, the unneeded inputs of a Switch node. */
  Set<const bNodeSocket *> unneeded_inputs;
};

/* Computes the execution schedule of the node group with the given instance key, assuming the
 * active node group has the given active instance key. Only outputs types and node group outputs
 * that are need are computed. This is essentially a post-order depth first traversal of the node
 * tree from the needed output nodes to the leaf input nodes, with informed order of traversal of
 * dependencies based on a heuristic estimation of the number of needed buffers. */
Schedule compute_schedule(const Context &context,
                          const bNodeTree &node_group,
                          NodeGroupOperation &node_group_operation,
                          const NodeGroupOutputTypes needed_outputs_types,
                          const bNodeInstanceKey instance_key,
                          const bNodeInstanceKey active_node_group_instance_key);

}  // namespace blender::compositor
