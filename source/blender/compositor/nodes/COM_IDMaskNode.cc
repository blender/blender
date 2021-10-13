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

#include "COM_IDMaskNode.h"
#include "COM_IDMaskOperation.h"
#include "COM_SMAAOperation.h"

namespace blender::compositor {

IDMaskNode::IDMaskNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}
void IDMaskNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext & /*context*/) const
{
  bNode *bnode = this->get_bnode();

  IDMaskOperation *operation;
  operation = new IDMaskOperation();
  operation->set_object_index(bnode->custom1);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  if (bnode->custom2 == 0) {
    converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
  }
  else {
    SMAAEdgeDetectionOperation *operation1 = nullptr;

    operation1 = new SMAAEdgeDetectionOperation();
    converter.add_operation(operation1);

    converter.add_link(operation->get_output_socket(0), operation1->get_input_socket(0));

    /* Blending Weight Calculation Pixel Shader (Second Pass). */
    SMAABlendingWeightCalculationOperation *operation2 =
        new SMAABlendingWeightCalculationOperation();
    converter.add_operation(operation2);

    converter.add_link(operation1->get_output_socket(), operation2->get_input_socket(0));

    /* Neighborhood Blending Pixel Shader (Third Pass). */
    SMAANeighborhoodBlendingOperation *operation3 = new SMAANeighborhoodBlendingOperation();
    converter.add_operation(operation3);

    converter.add_link(operation->get_output_socket(0), operation3->get_input_socket(0));
    converter.add_link(operation2->get_output_socket(), operation3->get_input_socket(1));
    converter.map_output_socket(get_output_socket(0), operation3->get_output_socket());
  }
}

}  // namespace blender::compositor
