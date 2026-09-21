/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_context.hh"
#include "COM_node_operation.hh"
#include "COM_node_tree_output_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* A node operation representing a node tree output node that for each of its inputs gets the input
 * and shares its data with the result of the node group operation it represents with the same
 * identifier. Node tree output nodes include group output and zone output nodes. */
class NodeTreeOutputNodeOperation : public NodeOperation {
 private:
  Operation &operation_;

 public:
  NodeTreeOutputNodeOperation(Context &context, const bNode &node, Operation &operation)
      : NodeOperation(context, node), operation_(operation)
  {
    for (const bNodeSocket *input : node.input_sockets()) {
      if (!is_socket_available(input)) {
        continue;
      }

      InputDescriptor &descriptor = this->get_input_descriptor(input->identifier);
      /* The structure type of the inputs of Group Output nodes are inferred, so we need to
       * make sure this is not wrongly expecting single values. */
      descriptor.expects_single_value = false;
      /* Groups Output nodes should not force realization since it is defined by the user, and
       * there is currently no way for the user to define that through the UI. */
      descriptor.realization_mode = InputRealizationMode::None;
    }
  }

  void execute() override
  {
    for (const bNodeSocket *input_socket : this->node().input_sockets()) {
      if (!is_socket_available(input_socket)) {
        continue;
      }

      Result &operation_result = operation_.get_result(input_socket->identifier);
      if (operation_result.should_compute()) {
        const Result &input_result = this->get_input(input_socket->identifier);
        operation_result.share_data(input_result);
      }
    }
  }
};

NodeOperation *get_node_tree_output_node_operation(Context &context,
                                                   const bNode &node,
                                                   Operation &operation)
{
  return new NodeTreeOutputNodeOperation(context, node, operation);
}

}  // namespace blender::compositor
