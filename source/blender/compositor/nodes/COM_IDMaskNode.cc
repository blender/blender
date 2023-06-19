/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  const bNode *bnode = this->get_bnode();

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
