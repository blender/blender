/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_context.hh"
#include "COM_group_output_node_operation.hh"
#include "COM_node_group_operation.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* A node operation representing a group output node that for each of its inputs gets the input
 * and shares its data with the result of the node group operation it represents with the same
 * identifier. */
class GroupOutputNodeOperation : public NodeOperation {
 private:
  NodeGroupOperation &node_group_operation_;

 public:
  GroupOutputNodeOperation(Context &context,
                           const bNode &node,
                           NodeGroupOperation &node_group_operation)
      : NodeOperation(context, node), node_group_operation_(node_group_operation)
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

      const Result &input_result = this->get_input(input_socket->identifier);
      Result &node_group_operation_result = node_group_operation_.get_result(
          input_socket->identifier);
      if (node_group_operation_result.should_compute()) {
        node_group_operation_result.share_data(input_result);
      }
    }
  }
};

NodeOperation *get_group_output_node_operation(Context &context,
                                               const bNode &node,
                                               NodeGroupOperation &node_group_operation)
{
  return new GroupOutputNodeOperation(context, node, node_group_operation);
}

}  // namespace blender::compositor
