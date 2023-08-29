/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorSpillNode.h"
#include "COM_ColorSpillOperation.h"

namespace blender::compositor {

ColorSpillNode::ColorSpillNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void ColorSpillNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  const bNode *editorsnode = get_bnode();

  NodeInput *input_socket_image = this->get_input_socket(0);
  NodeInput *input_socket_fac = this->get_input_socket(1);
  NodeOutput *output_socket_image = this->get_output_socket(0);

  ColorSpillOperation *operation;
  operation = new ColorSpillOperation();
  operation->set_settings((NodeColorspill *)editorsnode->storage);
  operation->set_spill_channel(editorsnode->custom1 - 1); /* Channel for spilling */
  operation->set_spill_method(editorsnode->custom2);      /* Channel method */
  converter.add_operation(operation);

  converter.map_input_socket(input_socket_image, operation->get_input_socket(0));
  converter.map_input_socket(input_socket_fac, operation->get_input_socket(1));
  converter.map_output_socket(output_socket_image, operation->get_output_socket());
}

}  // namespace blender::compositor
