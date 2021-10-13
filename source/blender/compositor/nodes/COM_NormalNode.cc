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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_NormalNode.h"
#include "COM_DotproductOperation.h"
#include "COM_SetVectorOperation.h"

namespace blender::compositor {

NormalNode::NormalNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void NormalNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext & /*context*/) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);
  NodeOutput *output_socket_dotproduct = this->get_output_socket(1);

  SetVectorOperation *operation_set = new SetVectorOperation();
  float normal[3];
  output_socket->get_editor_value_vector(normal);
  /* animation can break normalization, this restores it */
  normalize_v3(normal);
  operation_set->setX(normal[0]);
  operation_set->setY(normal[1]);
  operation_set->setZ(normal[2]);
  operation_set->setW(0.0f);
  converter.add_operation(operation_set);

  converter.map_output_socket(output_socket, operation_set->get_output_socket(0));

  DotproductOperation *operation = new DotproductOperation();
  converter.add_operation(operation);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.add_link(operation_set->get_output_socket(0), operation->get_input_socket(1));
  converter.map_output_socket(output_socket_dotproduct, operation->get_output_socket(0));
}

}  // namespace blender::compositor
