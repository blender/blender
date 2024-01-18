/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PixelateNode.h"

#include "COM_PixelateOperation.h"

namespace blender::compositor {

PixelateNode::PixelateNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void PixelateNode::convert_to_operations(NodeConverter &converter,
                                         const CompositorContext & /*context*/) const
{
  const bNode *editor_node = this->get_bnode();

  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);

  PixelateOperation *operation = new PixelateOperation();
  converter.add_operation(operation);

  operation->set_pixel_size(editor_node->custom1);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
}

}  // namespace blender::compositor
