/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2021, Blender Foundation.
 */

#include "COM_CombineXYZNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

CombineXYZNode::CombineXYZNode(bNode *editor_node) : Node(editor_node)
{
}

void CombineXYZNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext &UNUSED(context)) const
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
