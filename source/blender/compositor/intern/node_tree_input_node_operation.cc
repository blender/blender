/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_context.hh"
#include "COM_node_operation.hh"
#include "COM_node_tree_input_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* A node operation representing a node tree input node that for each of its outputs gets the input
 * from the node group operation it represents and shares its data with its own output with the
 * same identifier. Node tree input nodes include group input and zone input nodes. */
class NodeTreeInputNodeOperation : public NodeOperation {
 private:
  /* The operation that this input node belongs to. */
  Operation &operation_;

 public:
  NodeTreeInputNodeOperation(Context &context, const bNode &node, Operation &operation)
      : NodeOperation(context, node), operation_(operation)
  {
  }

  void execute() override
  {
    for (const bNodeSocket *output_socket : this->node().output_sockets()) {
      if (!is_socket_available(output_socket)) {
        continue;
      }

      Result &output_result = this->get_result(output_socket->identifier);
      if (output_result.should_compute()) {
        const Result &operation_input = operation_.get_input(output_socket->identifier);
        output_result.share_data(operation_input);
      }
    }
  }
};

NodeOperation *get_node_tree_input_node_operation(Context &context,
                                                  const bNode &node,
                                                  Operation &operation)
{
  return new NodeTreeInputNodeOperation(context, node, operation);
}

}  // namespace blender::compositor
