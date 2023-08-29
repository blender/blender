/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorNode.h"
#include "COM_SetColorOperation.h"

namespace blender::compositor {

ColorNode::ColorNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorNode::convert_to_operations(NodeConverter &converter,
                                      const CompositorContext & /*context*/) const
{
  SetColorOperation *operation = new SetColorOperation();
  NodeOutput *output = this->get_output_socket(0);
  float col[4];
  output->get_editor_value_color(col);
  operation->set_channels(col);
  converter.add_operation(operation);

  converter.map_output_socket(output, operation->get_output_socket());
}

}  // namespace blender::compositor
