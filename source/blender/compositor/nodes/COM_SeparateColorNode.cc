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

#include "COM_SeparateColorNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

SeparateColorNode::SeparateColorNode(bNode *editor_node) : Node(editor_node)
{
}

void SeparateColorNode::convert_to_operations(NodeConverter &converter,
                                              const CompositorContext &context) const
{
  NodeInput *image_socket = this->get_input_socket(0);
  NodeOutput *output_rsocket = this->get_output_socket(0);
  NodeOutput *output_gsocket = this->get_output_socket(1);
  NodeOutput *output_bsocket = this->get_output_socket(2);
  NodeOutput *output_asocket = this->get_output_socket(3);

  NodeOperation *color_conv = get_color_converter(context);
  if (color_conv) {
    converter.add_operation(color_conv);

    converter.map_input_socket(image_socket, color_conv->get_input_socket(0));
  }

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(0);
    converter.add_operation(operation);

    if (color_conv) {
      converter.add_link(color_conv->get_output_socket(), operation->get_input_socket(0));
    }
    else {
      converter.map_input_socket(image_socket, operation->get_input_socket(0));
    }
    converter.map_output_socket(output_rsocket, operation->get_output_socket(0));
  }

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(1);
    converter.add_operation(operation);

    if (color_conv) {
      converter.add_link(color_conv->get_output_socket(), operation->get_input_socket(0));
    }
    else {
      converter.map_input_socket(image_socket, operation->get_input_socket(0));
    }
    converter.map_output_socket(output_gsocket, operation->get_output_socket(0));
  }

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(2);
    converter.add_operation(operation);

    if (color_conv) {
      converter.add_link(color_conv->get_output_socket(), operation->get_input_socket(0));
    }
    else {
      converter.map_input_socket(image_socket, operation->get_input_socket(0));
    }
    converter.map_output_socket(output_bsocket, operation->get_output_socket(0));
  }

  {
    SeparateChannelOperation *operation = new SeparateChannelOperation();
    operation->set_channel(3);
    converter.add_operation(operation);

    if (color_conv) {
      converter.add_link(color_conv->get_output_socket(), operation->get_input_socket(0));
    }
    else {
      converter.map_input_socket(image_socket, operation->get_input_socket(0));
    }
    converter.map_output_socket(output_asocket, operation->get_output_socket(0));
  }
}

NodeOperation *SeparateRGBANode::get_color_converter(const CompositorContext & /*context*/) const
{
  return nullptr; /* no conversion needed */
}

NodeOperation *SeparateHSVANode::get_color_converter(const CompositorContext & /*context*/) const
{
  return new ConvertRGBToHSVOperation();
}

NodeOperation *SeparateYCCANode::get_color_converter(const CompositorContext & /*context*/) const
{
  ConvertRGBToYCCOperation *operation = new ConvertRGBToYCCOperation();
  bNode *editor_node = this->get_bnode();
  operation->set_mode(editor_node->custom1);
  return operation;
}

NodeOperation *SeparateYUVANode::get_color_converter(const CompositorContext & /*context*/) const
{
  return new ConvertRGBToYUVOperation();
}

}  // namespace blender::compositor
