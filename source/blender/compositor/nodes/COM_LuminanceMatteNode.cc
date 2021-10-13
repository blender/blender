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

#include "COM_LuminanceMatteNode.h"
#include "COM_LuminanceMatteOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

namespace blender::compositor {

LuminanceMatteNode::LuminanceMatteNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void LuminanceMatteNode::convert_to_operations(NodeConverter &converter,
                                               const CompositorContext & /*context*/) const
{
  bNode *editorsnode = get_bnode();
  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket_image = this->get_output_socket(0);
  NodeOutput *output_socket_matte = this->get_output_socket(1);

  LuminanceMatteOperation *operation_set = new LuminanceMatteOperation();
  operation_set->set_settings((NodeChroma *)editorsnode->storage);
  converter.add_operation(operation_set);

  converter.map_input_socket(input_socket, operation_set->get_input_socket(0));
  converter.map_output_socket(output_socket_matte, operation_set->get_output_socket(0));

  SetAlphaMultiplyOperation *operation = new SetAlphaMultiplyOperation();
  converter.add_operation(operation);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.add_link(operation_set->get_output_socket(), operation->get_input_socket(1));
  converter.map_output_socket(output_socket_image, operation->get_output_socket());

  converter.add_preview(operation->get_output_socket());
}

}  // namespace blender::compositor
