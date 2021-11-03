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

#include "COM_CombineColorNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

CombineColorNode::CombineColorNode(bNode *editor_node) : Node(editor_node)
{
}

void CombineColorNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext &context) const
{
  NodeInput *input_rsocket = this->get_input_socket(0);
  NodeInput *input_gsocket = this->get_input_socket(1);
  NodeInput *input_bsocket = this->get_input_socket(2);
  NodeInput *input_asocket = this->get_input_socket(3);
  NodeOutput *output_socket = this->get_output_socket(0);

  CombineChannelsOperation *operation = new CombineChannelsOperation();
  if (input_rsocket->is_linked()) {
    operation->set_canvas_input_index(0);
  }
  else if (input_gsocket->is_linked()) {
    operation->set_canvas_input_index(1);
  }
  else if (input_bsocket->is_linked()) {
    operation->set_canvas_input_index(2);
  }
  else {
    operation->set_canvas_input_index(3);
  }
  converter.add_operation(operation);

  converter.map_input_socket(input_rsocket, operation->get_input_socket(0));
  converter.map_input_socket(input_gsocket, operation->get_input_socket(1));
  converter.map_input_socket(input_bsocket, operation->get_input_socket(2));
  converter.map_input_socket(input_asocket, operation->get_input_socket(3));

  NodeOperation *color_conv = get_color_converter(context);
  if (color_conv) {
    converter.add_operation(color_conv);

    converter.add_link(operation->get_output_socket(), color_conv->get_input_socket(0));
    converter.map_output_socket(output_socket, color_conv->get_output_socket());
  }
  else {
    converter.map_output_socket(output_socket, operation->get_output_socket());
  }
}

NodeOperation *CombineRGBANode::get_color_converter(const CompositorContext & /*context*/) const
{
  return nullptr; /* no conversion needed */
}

NodeOperation *CombineHSVANode::get_color_converter(const CompositorContext & /*context*/) const
{
  return new ConvertHSVToRGBOperation();
}

NodeOperation *CombineYCCANode::get_color_converter(const CompositorContext & /*context*/) const
{
  ConvertYCCToRGBOperation *operation = new ConvertYCCToRGBOperation();
  bNode *editor_node = this->get_bnode();
  operation->set_mode(editor_node->custom1);
  return operation;
}

NodeOperation *CombineYUVANode::get_color_converter(const CompositorContext & /*context*/) const
{
  return new ConvertYUVToRGBOperation();
}

}  // namespace blender::compositor
