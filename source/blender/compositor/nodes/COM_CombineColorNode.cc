/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CombineColorNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

CombineColorNode::CombineColorNode(bNode *editor_node) : Node(editor_node) {}

void CombineColorNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext & /*context*/) const
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

  const bNode *editor_node = this->get_bnode();
  NodeCMPCombSepColor *storage = (NodeCMPCombSepColor *)editor_node->storage;

  NodeOperation *color_conv = nullptr;
  switch (storage->mode) {
    case CMP_NODE_COMBSEP_COLOR_RGB: {
      /* Pass */
      break;
    }
    case CMP_NODE_COMBSEP_COLOR_HSV: {
      color_conv = new ConvertHSVToRGBOperation();
      break;
    }
    case CMP_NODE_COMBSEP_COLOR_HSL: {
      color_conv = new ConvertHSLToRGBOperation();
      break;
    }
    case CMP_NODE_COMBSEP_COLOR_YCC: {
      ConvertYCCToRGBOperation *operation = new ConvertYCCToRGBOperation();
      operation->set_mode(storage->ycc_mode);
      color_conv = operation;
      break;
    }
    case CMP_NODE_COMBSEP_COLOR_YUV: {
      color_conv = new ConvertYUVToRGBOperation();
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }

  if (color_conv) {
    converter.add_operation(color_conv);

    converter.add_link(operation->get_output_socket(), color_conv->get_input_socket(0));
    converter.map_output_socket(output_socket, color_conv->get_output_socket());
  }
  else {
    converter.map_output_socket(output_socket, operation->get_output_socket());
  }
}

}  // namespace blender::compositor
