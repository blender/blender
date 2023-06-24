/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
