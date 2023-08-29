/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SeparateColorNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

SeparateColorNode::SeparateColorNode(bNode *editor_node) : Node(editor_node) {}

void SeparateColorNode::convert_to_operations(NodeConverter &converter,
                                              const CompositorContext & /*context*/) const
{
  NodeInput *image_socket = this->get_input_socket(0);
  NodeOutput *output_rsocket = this->get_output_socket(0);
  NodeOutput *output_gsocket = this->get_output_socket(1);
  NodeOutput *output_bsocket = this->get_output_socket(2);
  NodeOutput *output_asocket = this->get_output_socket(3);

  const bNode *editor_node = this->get_bnode();
  const NodeCMPCombSepColor *storage = (const NodeCMPCombSepColor *)editor_node->storage;

  NodeOperation *color_conv = nullptr;
  switch (storage->mode) {
    case CMP_NODE_COMBSEP_COLOR_RGB: {
      /* Pass */
      break;
    }
    case CMP_NODE_COMBSEP_COLOR_HSV: {
      color_conv = new ConvertRGBToHSVOperation();
      break;
    }
    case CMP_NODE_COMBSEP_COLOR_HSL: {
      color_conv = new ConvertRGBToHSLOperation();
      break;
    }
    case CMP_NODE_COMBSEP_COLOR_YCC: {
      ConvertRGBToYCCOperation *operation = new ConvertRGBToYCCOperation();
      operation->set_mode(storage->ycc_mode);
      color_conv = operation;
      break;
    }
    case CMP_NODE_COMBSEP_COLOR_YUV: {
      color_conv = new ConvertRGBToYUVOperation();
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }

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

}  // namespace blender::compositor
