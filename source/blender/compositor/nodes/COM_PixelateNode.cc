/* SPDX-FileCopyrightText: 2011 Blender Foundation.
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
  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);
  DataType datatype = input_socket->get_data_type();

  if (input_socket->is_linked()) {
    NodeOutput *link = input_socket->get_link();
    datatype = link->get_data_type();
  }

  PixelateOperation *operation = new PixelateOperation(datatype);
  converter.add_operation(operation);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
}

}  // namespace blender::compositor
