/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorToBWNode.h"

#include "COM_ConvertOperation.h"

namespace blender::compositor {

ColorToBWNode::ColorToBWNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorToBWNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  NodeInput *color_socket = this->get_input_socket(0);
  NodeOutput *value_socket = this->get_output_socket(0);

  ConvertColorToBWOperation *convert_prog = new ConvertColorToBWOperation();
  converter.add_operation(convert_prog);

  converter.map_input_socket(color_socket, convert_prog->get_input_socket(0));
  converter.map_output_socket(value_socket, convert_prog->get_output_socket(0));
}

}  // namespace blender::compositor
