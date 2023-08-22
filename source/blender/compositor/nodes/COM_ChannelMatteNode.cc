/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ChannelMatteNode.h"
#include "COM_ChannelMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

#include "BLI_math_color.h"

namespace blender::compositor {

ChannelMatteNode::ChannelMatteNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ChannelMatteNode::convert_to_operations(NodeConverter &converter,
                                             const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();

  NodeInput *input_socket_image = this->get_input_socket(0);
  NodeOutput *output_socket_image = this->get_output_socket(0);
  NodeOutput *output_socket_matte = this->get_output_socket(1);

  NodeOperation *convert = nullptr, *inv_convert = nullptr;
  /* color-space */
  switch (node->custom1) {
    case CMP_NODE_CHANNEL_MATTE_CS_RGB:
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_HSV: /* HSV */
      convert = new ConvertRGBToHSVOperation();
      inv_convert = new ConvertHSVToRGBOperation();
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_YUV: /* YUV */
      convert = new ConvertRGBToYUVOperation();
      inv_convert = new ConvertYUVToRGBOperation();
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_YCC: /* YCC */
      convert = new ConvertRGBToYCCOperation();
      ((ConvertRGBToYCCOperation *)convert)->set_mode(BLI_YCC_ITU_BT709);
      inv_convert = new ConvertYCCToRGBOperation();
      ((ConvertYCCToRGBOperation *)inv_convert)->set_mode(BLI_YCC_ITU_BT709);
      break;
    default:
      break;
  }

  ChannelMatteOperation *operation = new ChannelMatteOperation();
  /* pass the ui properties to the operation */
  operation->set_settings((NodeChroma *)node->storage, node->custom2);
  converter.add_operation(operation);

  SetAlphaMultiplyOperation *operation_alpha = new SetAlphaMultiplyOperation();
  converter.add_operation(operation_alpha);

  if (convert != nullptr) {
    converter.add_operation(convert);

    converter.map_input_socket(input_socket_image, convert->get_input_socket(0));
    converter.add_link(convert->get_output_socket(), operation->get_input_socket(0));
    converter.add_link(convert->get_output_socket(), operation_alpha->get_input_socket(0));
  }
  else {
    converter.map_input_socket(input_socket_image, operation->get_input_socket(0));
    converter.map_input_socket(input_socket_image, operation_alpha->get_input_socket(0));
  }

  converter.map_output_socket(output_socket_matte, operation->get_output_socket(0));
  converter.add_link(operation->get_output_socket(), operation_alpha->get_input_socket(1));

  if (inv_convert != nullptr) {
    converter.add_operation(inv_convert);
    converter.add_link(operation_alpha->get_output_socket(0), inv_convert->get_input_socket(0));
    converter.map_output_socket(output_socket_image, inv_convert->get_output_socket());
    converter.add_preview(inv_convert->get_output_socket());
  }
  else {
    converter.map_output_socket(output_socket_image, operation_alpha->get_output_socket());
    converter.add_preview(operation_alpha->get_output_socket());
  }
}

}  // namespace blender::compositor
