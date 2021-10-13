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

#include "COM_DistanceMatteNode.h"
#include "COM_ConvertOperation.h"
#include "COM_DistanceYCCMatteOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

namespace blender::compositor {

DistanceMatteNode::DistanceMatteNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DistanceMatteNode::convert_to_operations(NodeConverter &converter,
                                              const CompositorContext & /*context*/) const
{
  bNode *editorsnode = get_bnode();
  NodeChroma *storage = (NodeChroma *)editorsnode->storage;

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
