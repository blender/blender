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

#include "COM_ColorRampNode.h"
#include "COM_ColorRampOperation.h"
#include "COM_ConvertOperation.h"

namespace blender::compositor {

ColorRampNode::ColorRampNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorRampNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);
  NodeOutput *output_socket_alpha = this->get_output_socket(1);
  bNode *editor_node = this->get_bnode();

  ColorRampOperation *operation = new ColorRampOperation();
  operation->set_color_band((ColorBand *)editor_node->storage);
  converter.add_operation(operation);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));

  SeparateChannelOperation *operation2 = new SeparateChannelOperation();
  operation2->set_channel(3);
  converter.add_operation(operation2);

  converter.add_link(operation->get_output_socket(), operation2->get_input_socket(0));
  converter.map_output_socket(output_socket_alpha, operation2->get_output_socket());
}

}  // namespace blender::compositor
