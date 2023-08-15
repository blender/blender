/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ChromaMatteNode.h"
#include "COM_ChromaMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

namespace blender::compositor {

ChromaMatteNode::ChromaMatteNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ChromaMatteNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext & /*context*/) const
{
  const bNode *editorsnode = get_bnode();

  NodeInput *input_socket_image = this->get_input_socket(0);
  NodeInput *input_socket_key = this->get_input_socket(1);
  NodeOutput *output_socket_image = this->get_output_socket(0);
  NodeOutput *output_socket_matte = this->get_output_socket(1);

  ConvertRGBToYCCOperation *operationRGBToYCC_Image = new ConvertRGBToYCCOperation();
  ConvertRGBToYCCOperation *operationRGBToYCC_Key = new ConvertRGBToYCCOperation();
  operationRGBToYCC_Image->set_mode(BLI_YCC_ITU_BT709);
  operationRGBToYCC_Key->set_mode(BLI_YCC_ITU_BT709);
  converter.add_operation(operationRGBToYCC_Image);
  converter.add_operation(operationRGBToYCC_Key);

  ChromaMatteOperation *operation = new ChromaMatteOperation();
  operation->set_settings((NodeChroma *)editorsnode->storage);
  converter.add_operation(operation);

  SetAlphaMultiplyOperation *operation_alpha = new SetAlphaMultiplyOperation();
  converter.add_operation(operation_alpha);

  converter.map_input_socket(input_socket_image, operationRGBToYCC_Image->get_input_socket(0));
  converter.map_input_socket(input_socket_key, operationRGBToYCC_Key->get_input_socket(0));
  converter.add_link(operationRGBToYCC_Image->get_output_socket(), operation->get_input_socket(0));
  converter.add_link(operationRGBToYCC_Key->get_output_socket(), operation->get_input_socket(1));
  converter.map_output_socket(output_socket_matte, operation->get_output_socket());

  converter.map_input_socket(input_socket_image, operation_alpha->get_input_socket(0));
  converter.add_link(operation->get_output_socket(), operation_alpha->get_input_socket(1));
  converter.map_output_socket(output_socket_image, operation_alpha->get_output_socket());

  converter.add_preview(operation_alpha->get_output_socket());
}

}  // namespace blender::compositor
