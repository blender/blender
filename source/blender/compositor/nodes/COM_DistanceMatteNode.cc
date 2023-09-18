/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DistanceMatteNode.h"
#include "COM_ConvertOperation.h"
#include "COM_DistanceYCCMatteOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

#include "BLI_math_color.h"

namespace blender::compositor {

DistanceMatteNode::DistanceMatteNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DistanceMatteNode::convert_to_operations(NodeConverter &converter,
                                              const CompositorContext & /*context*/) const
{
  const bNode *editorsnode = this->get_bnode();
  const NodeChroma *storage = (const NodeChroma *)editorsnode->storage;

  NodeInput *input_socket_image = this->get_input_socket(0);
  NodeInput *input_socket_key = this->get_input_socket(1);
  NodeOutput *output_socket_image = this->get_output_socket(0);
  NodeOutput *output_socket_matte = this->get_output_socket(1);

  SetAlphaMultiplyOperation *operation_alpha = new SetAlphaMultiplyOperation();
  converter.add_operation(operation_alpha);

  /* work in RGB color space */
  NodeOperation *operation;
  if (storage->channel == 1) {
    DistanceRGBMatteOperation *matte = new DistanceRGBMatteOperation();
    matte->set_settings(storage);
    converter.add_operation(matte);

    converter.map_input_socket(input_socket_image, matte->get_input_socket(0));
    converter.map_input_socket(input_socket_image, operation_alpha->get_input_socket(0));

    converter.map_input_socket(input_socket_key, matte->get_input_socket(1));

    operation = matte;
  }
  /* work in YCbCr color space */
  else {
    DistanceYCCMatteOperation *matte = new DistanceYCCMatteOperation();
    matte->set_settings(storage);
    converter.add_operation(matte);

    ConvertRGBToYCCOperation *operation_yccimage = new ConvertRGBToYCCOperation();
    ConvertRGBToYCCOperation *operation_yccmatte = new ConvertRGBToYCCOperation();
    operation_yccimage->set_mode(BLI_YCC_ITU_BT709);
    operation_yccmatte->set_mode(BLI_YCC_ITU_BT709);
    converter.add_operation(operation_yccimage);
    converter.add_operation(operation_yccmatte);

    converter.map_input_socket(input_socket_image, operation_yccimage->get_input_socket(0));
    converter.add_link(operation_yccimage->get_output_socket(), matte->get_input_socket(0));
    converter.add_link(operation_yccimage->get_output_socket(),
                       operation_alpha->get_input_socket(0));

    converter.map_input_socket(input_socket_key, operation_yccmatte->get_input_socket(0));
    converter.add_link(operation_yccmatte->get_output_socket(), matte->get_input_socket(1));

    operation = matte;
  }

  converter.map_output_socket(output_socket_matte, operation->get_output_socket(0));
  converter.add_link(operation->get_output_socket(), operation_alpha->get_input_socket(1));

  if (storage->channel != 1) {
    ConvertYCCToRGBOperation *inv_convert = new ConvertYCCToRGBOperation();
    inv_convert->set_mode(BLI_YCC_ITU_BT709);

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
