/* SPDX-FileCopyrightText: 2012 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_NormalizeNode.h"
#include "COM_NormalizeOperation.h"

namespace blender::compositor {

NormalizeNode::NormalizeNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void NormalizeNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  NormalizeOperation *operation = new NormalizeOperation();
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
