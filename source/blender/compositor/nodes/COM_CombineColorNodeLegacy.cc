/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CombineColorNodeLegacy.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

CombineColorNodeLegacy::CombineColorNodeLegacy(bNode *editor_node) : Node(editor_node) {}

void CombineColorNodeLegacy::convert_to_operations(NodeConverter &converter,
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
  const bNode *editor_node = this->get_bnode();
  operation->set_mode(editor_node->custom1);
  return operation;
}

NodeOperation *CombineYUVANode::get_color_converter(const CompositorContext & /*context*/) const
{
  return new ConvertYUVToRGBOperation();
}

}  // namespace blender::compositor
