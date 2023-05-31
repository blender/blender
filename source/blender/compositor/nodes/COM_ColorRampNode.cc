/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorRampNode.h"
#include "COM_ColorRampOperation.h"
#include "COM_ConvertOperation.h"

namespace blender::compositor {

ColorRampNode::ColorRampNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorRampNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);
  NodeOutput *output_socket_alpha = this->get_output_socket(1);
  const bNode *editor_node = this->get_bnode();

  ColorRampOperation *operation = new ColorRampOperation();
  operation->set_color_band((ColorBand *)editor_node->storage);
  converter.add_operation(operation);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));

  SeparateChannelOperation *operation2 = new SeparateChannelOperation();
  operation2->set_channel(3);
  converter.add_operation(operation2);

  converter.add_link(operation->get_output_socket(), operation2->get_input_socket(0));
  converter.map_output_socket(output_socket_alpha, operation2->get_output_socket());
}

}  // namespace blender::compositor
