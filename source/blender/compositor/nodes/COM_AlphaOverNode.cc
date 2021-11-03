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

#include "COM_AlphaOverNode.h"

#include "COM_AlphaOverKeyOperation.h"
#include "COM_AlphaOverMixedOperation.h"
#include "COM_AlphaOverPremultiplyOperation.h"

namespace blender::compositor {

void AlphaOverNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  NodeInput *color1Socket = this->get_input_socket(1);
  NodeInput *color2Socket = this->get_input_socket(2);
  bNode *editor_node = this->get_bnode();

  MixBaseOperation *convert_prog;
  NodeTwoFloats *ntf = (NodeTwoFloats *)editor_node->storage;
  if (ntf->x != 0.0f) {
    AlphaOverMixedOperation *mix_operation = new AlphaOverMixedOperation();
    mix_operation->setX(ntf->x);
    convert_prog = mix_operation;
  }
  else if (editor_node->custom1) {
    convert_prog = new AlphaOverKeyOperation();
  }
  else {
    convert_prog = new AlphaOverPremultiplyOperation();
  }

  convert_prog->set_use_value_alpha_multiply(false);
  if (color1Socket->is_linked()) {
    convert_prog->set_canvas_input_index(1);
  }
  else if (color2Socket->is_linked()) {
    convert_prog->set_canvas_input_index(2);
  }
  else {
    convert_prog->set_canvas_input_index(0);
  }

  converter.add_operation(convert_prog);
  converter.map_input_socket(get_input_socket(0), convert_prog->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), convert_prog->get_input_socket(1));
  converter.map_input_socket(get_input_socket(2), convert_prog->get_input_socket(2));
  converter.map_output_socket(get_output_socket(0), convert_prog->get_output_socket(0));
}

}  // namespace blender::compositor
