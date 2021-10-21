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

#include "COM_HueSaturationValueNode.h"

#include "COM_ChangeHSVOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_MixOperation.h"

namespace blender::compositor {

HueSaturationValueNode::HueSaturationValueNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void HueSaturationValueNode::convert_to_operations(NodeConverter &converter,
                                                   const CompositorContext & /*context*/) const
{
  NodeInput *color_socket = this->get_input_socket(0);
  NodeInput *hue_socket = this->get_input_socket(1);
  NodeInput *saturation_socket = this->get_input_socket(2);
  NodeInput *value_socket = this->get_input_socket(3);
  NodeInput *fac_socket = this->get_input_socket(4);
  NodeOutput *output_socket = this->get_output_socket(0);

  ConvertRGBToHSVOperation *rgbToHSV = new ConvertRGBToHSVOperation();
  converter.add_operation(rgbToHSV);

  ConvertHSVToRGBOperation *hsvToRGB = new ConvertHSVToRGBOperation();
  converter.add_operation(hsvToRGB);

  ChangeHSVOperation *changeHSV = new ChangeHSVOperation();
  converter.map_input_socket(hue_socket, changeHSV->get_input_socket(1));
  converter.map_input_socket(saturation_socket, changeHSV->get_input_socket(2));
  converter.map_input_socket(value_socket, changeHSV->get_input_socket(3));
  converter.add_operation(changeHSV);

  MixBlendOperation *blend = new MixBlendOperation();
  blend->set_canvas_input_index(1);
  converter.add_operation(blend);

  converter.map_input_socket(color_socket, rgbToHSV->get_input_socket(0));
  converter.add_link(rgbToHSV->get_output_socket(), changeHSV->get_input_socket(0));
  converter.add_link(changeHSV->get_output_socket(), hsvToRGB->get_input_socket(0));
  converter.add_link(hsvToRGB->get_output_socket(), blend->get_input_socket(2));
  converter.map_input_socket(color_socket, blend->get_input_socket(1));
  converter.map_input_socket(fac_socket, blend->get_input_socket(0));
  converter.map_output_socket(output_socket, blend->get_output_socket());
}

}  // namespace blender::compositor
