/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SeparateXYZNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

SeparateXYZNode::SeparateXYZNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SeparateXYZNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext & /*context*/) const
{
  NodeInput *vector_socket = this->get_input_socket(0);
  NodeOutput *output_x_socket = this->get_output_socket(0);
  NodeOutput *output_y_socket = this->get_output_socket(1);
  NodeOutput *output_z_socket = this->get_output_socket(2);

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(0);
    converter.add_operation(operation);
    converter.map_input_socket(vector_socket, operation->get_input_socket(0));
    converter.map_output_socket(output_x_socket, operation->get_output_socket(0));
  }

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(1);
    converter.add_operation(operation);
    converter.map_input_socket(vector_socket, operation->get_input_socket(0));
    converter.map_output_socket(output_y_socket, operation->get_output_socket(0));
  }

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(2);
    converter.add_operation(operation);
    converter.map_input_socket(vector_socket, operation->get_input_socket(0));
    converter.map_output_socket(output_z_socket, operation->get_output_socket(0));
  }
}

}  // namespace blender::compositor
