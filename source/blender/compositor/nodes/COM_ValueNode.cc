/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ValueNode.h"
#include "COM_SetValueOperation.h"

namespace blender::compositor {

ValueNode::ValueNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ValueNode::convert_to_operations(NodeConverter &converter,
                                      const CompositorContext & /*context*/) const
{
  SetValueOperation *operation = new SetValueOperation();
  NodeOutput *output = this->get_output_socket(0);
  operation->set_value(output->get_editor_value_float());
  converter.add_operation(operation);

  converter.map_output_socket(output, operation->get_output_socket());
}

}  // namespace blender::compositor
