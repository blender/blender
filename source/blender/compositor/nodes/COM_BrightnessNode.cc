/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BrightnessNode.h"
#include "COM_BrightnessOperation.h"

namespace blender::compositor {

BrightnessNode::BrightnessNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void BrightnessNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  const bNode *bnode = this->get_bnode();
  BrightnessOperation *operation = new BrightnessOperation();
  operation->set_use_premultiply((bnode->custom1 & 1) != 0);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
