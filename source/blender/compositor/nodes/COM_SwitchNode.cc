/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SwitchNode.h"

namespace blender::compositor {

SwitchNode::SwitchNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SwitchNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext & /*context*/) const
{
  bool condition = this->get_bnode()->custom1;

  NodeOperationOutput *result;
  if (!condition) {
    result = converter.add_input_proxy(get_input_socket(0), false);
  }
  else {
    result = converter.add_input_proxy(get_input_socket(1), false);
  }

  converter.map_output_socket(get_output_socket(0), result);
}

}  // namespace blender::compositor
