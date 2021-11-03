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

#include "COM_ViewLevelsNode.h"
#include "COM_CalculateStandardDeviationOperation.h"

namespace blender::compositor {

ViewLevelsNode::ViewLevelsNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ViewLevelsNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  NodeInput *input = this->get_input_socket(0);
  if (input->is_linked()) {
    /* Add preview to input-socket. */

    /* calculate mean operation */
    {
      CalculateMeanOperation *operation = new CalculateMeanOperation();
      operation->set_setting(this->get_bnode()->custom1);

      converter.add_operation(operation);
      converter.map_input_socket(input, operation->get_input_socket(0));
      converter.map_output_socket(this->get_output_socket(0), operation->get_output_socket());
    }

    /* calculate standard deviation operation */
    {
      CalculateStandardDeviationOperation *operation = new CalculateStandardDeviationOperation();
      operation->set_setting(this->get_bnode()->custom1);

      converter.add_operation(operation);
      converter.map_input_socket(input, operation->get_input_socket(0));
      converter.map_output_socket(this->get_output_socket(1), operation->get_output_socket());
    }
  }
  else {
    converter.add_output_value(get_output_socket(0), 0.0f);
    converter.add_output_value(get_output_socket(1), 0.0f);
  }
}

}  // namespace blender::compositor
