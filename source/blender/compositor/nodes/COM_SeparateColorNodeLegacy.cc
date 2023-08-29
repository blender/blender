/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SeparateColorNodeLegacy.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

SeparateColorNodeLegacy::SeparateColorNodeLegacy(bNode *editor_node) : Node(editor_node) {}

void SeparateColorNodeLegacy::convert_to_operations(NodeConverter &converter,
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
  const bNode *editor_node = this->get_bnode();
  operation->set_mode(editor_node->custom1);
  return operation;
}

NodeOperation *SeparateYUVANode::get_color_converter(const CompositorContext & /*context*/) const
{
  return new ConvertRGBToYUVOperation();
}

}  // namespace blender::compositor
