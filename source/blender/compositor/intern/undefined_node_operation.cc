/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_derived_node_tree.hh"

#include "BKE_node.hh"

#include "COM_context.hh"
#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_undefined_node_operation.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

class UndefinedNodeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    for (const bNodeSocket *output : this->node()->output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      Result &result = this->get_result(output->identifier);
      if (result.should_compute()) {
        result.allocate_invalid();
      }
    }
  }
};

NodeOperation *get_undefined_node_operation(Context &context, DNode node)
{
  return new UndefinedNodeOperation(context, node);
}

}  // namespace blender::compositor
