/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CombineXYZNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

CombineXYZNode::CombineXYZNode(bNode *editor_node) : Node(editor_node) {}

void CombineXYZNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  NodeInput *input_x_socket = this->get_input_socket(0);
  NodeInput *input_y_socket = this->get_input_socket(1);
  NodeInput *input_z_socket = this->get_input_socket(2);
  NodeOutput *output_socket = this->get_output_socket(0);

  CombineChannelsOperation *operation = new CombineChannelsOperation();
  if (input_x_socket->is_linked()) {
    operation->set_canvas_input_index(0);
  }
  else if (input_y_socket->is_linked()) {
    operation->set_canvas_input_index(1);
  }
  else {
    operation->set_canvas_input_index(2);
  }
  converter.add_operation(operation);

  converter.map_input_socket(input_x_socket, operation->get_input_socket(0));
  converter.map_input_socket(input_y_socket, operation->get_input_socket(1));
  converter.map_input_socket(input_z_socket, operation->get_input_socket(2));
  converter.map_output_socket(output_socket, operation->get_output_socket());
}

}  // namespace blender::compositor
