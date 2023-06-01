/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorMatteNode.h"
#include "COM_ColorMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

namespace blender::compositor {

ColorMatteNode::ColorMatteNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorMatteNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  const bNode *editorsnode = get_bnode();

  NodeInput *input_socket_image = this->get_input_socket(0);
  NodeInput *input_socket_key = this->get_input_socket(1);
  NodeOutput *output_socket_image = this->get_output_socket(0);
  NodeOutput *output_socket_matte = this->get_output_socket(1);

  ConvertRGBToHSVOperation *operationRGBToHSV_Image = new ConvertRGBToHSVOperation();
  ConvertRGBToHSVOperation *operationRGBToHSV_Key = new ConvertRGBToHSVOperation();
  converter.add_operation(operationRGBToHSV_Image);
  converter.add_operation(operationRGBToHSV_Key);

  ColorMatteOperation *operation = new ColorMatteOperation();
  operation->set_settings((NodeChroma *)editorsnode->storage);
  converter.add_operation(operation);

  SetAlphaMultiplyOperation *operation_alpha = new SetAlphaMultiplyOperation();
  converter.add_operation(operation_alpha);

  converter.map_input_socket(input_socket_image, operationRGBToHSV_Image->get_input_socket(0));
  converter.map_input_socket(input_socket_key, operationRGBToHSV_Key->get_input_socket(0));
  converter.add_link(operationRGBToHSV_Image->get_output_socket(), operation->get_input_socket(0));
  converter.add_link(operationRGBToHSV_Key->get_output_socket(), operation->get_input_socket(1));
  converter.map_output_socket(output_socket_matte, operation->get_output_socket(0));

  converter.map_input_socket(input_socket_image, operation_alpha->get_input_socket(0));
  converter.add_link(operation->get_output_socket(), operation_alpha->get_input_socket(1));
  converter.map_output_socket(output_socket_image, operation_alpha->get_output_socket());

  converter.add_preview(operation_alpha->get_output_socket());
}

}  // namespace blender::compositor
