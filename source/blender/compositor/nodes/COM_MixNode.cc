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

#include "COM_MixNode.h"

#include "COM_MixOperation.h"

#include "DNA_material_types.h" /* the ramp types */

namespace blender::compositor {

MixNode::MixNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void MixNode::convert_to_operations(NodeConverter &converter,
                                    const CompositorContext & /*context*/) const
{
  NodeInput *value_socket = this->get_input_socket(0);
  NodeInput *color1Socket = this->get_input_socket(1);
  NodeInput *color2Socket = this->get_input_socket(2);
  NodeOutput *output_socket = this->get_output_socket(0);
  bNode *editor_node = this->get_bnode();
  bool use_alpha_premultiply = (this->get_bnode()->custom2 & 1) != 0;
  bool use_clamp = (this->get_bnode()->custom2 & 2) != 0;

  MixBaseOperation *convert_prog;
  switch (editor_node->custom1) {
    case MA_RAMP_ADD:
      convert_prog = new MixAddOperation();
      break;
    case MA_RAMP_MULT:
      convert_prog = new MixMultiplyOperation();
      break;
    case MA_RAMP_LIGHT:
      convert_prog = new MixLightenOperation();
      break;
    case MA_RAMP_BURN:
      convert_prog = new MixColorBurnOperation();
      break;
    case MA_RAMP_HUE:
      convert_prog = new MixHueOperation();
      break;
    case MA_RAMP_COLOR:
      convert_prog = new MixColorOperation();
      break;
    case MA_RAMP_SOFT:
      convert_prog = new MixSoftLightOperation();
      break;
    case MA_RAMP_SCREEN:
      convert_prog = new MixScreenOperation();
      break;
    case MA_RAMP_LINEAR:
      convert_prog = new MixLinearLightOperation();
      break;
    case MA_RAMP_DIFF:
      convert_prog = new MixDifferenceOperation();
      break;
    case MA_RAMP_SAT:
      convert_prog = new MixSaturationOperation();
      break;
    case MA_RAMP_DIV:
      convert_prog = new MixDivideOperation();
      break;
    case MA_RAMP_SUB:
      convert_prog = new MixSubtractOperation();
      break;
    case MA_RAMP_DARK:
      convert_prog = new MixDarkenOperation();
      break;
    case MA_RAMP_OVERLAY:
      convert_prog = new MixOverlayOperation();
      break;
    case MA_RAMP_VAL:
      convert_prog = new MixValueOperation();
      break;
    case MA_RAMP_DODGE:
      convert_prog = new MixDodgeOperation();
      break;

    case MA_RAMP_BLEND:
    default:
      convert_prog = new MixBlendOperation();
      break;
  }
  convert_prog->set_use_value_alpha_multiply(use_alpha_premultiply);
  convert_prog->set_use_clamp(use_clamp);
  converter.add_operation(convert_prog);

  converter.map_input_socket(value_socket, convert_prog->get_input_socket(0));
  converter.map_input_socket(color1Socket, convert_prog->get_input_socket(1));
  converter.map_input_socket(color2Socket, convert_prog->get_input_socket(2));
  converter.map_output_socket(output_socket, convert_prog->get_output_socket(0));

  converter.add_preview(convert_prog->get_output_socket(0));
}

}  // namespace blender::compositor
