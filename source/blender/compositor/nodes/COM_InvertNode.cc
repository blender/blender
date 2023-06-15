/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_InvertNode.h"
#include "BKE_node.hh"
#include "COM_InvertOperation.h"

namespace blender::compositor {

InvertNode::InvertNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void InvertNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext & /*context*/) const
{
  InvertOperation *operation = new InvertOperation();
  const bNode *node = this->get_bnode();
  operation->set_color(node->custom1 & CMP_CHAN_RGB);
  operation->set_alpha(node->custom1 & CMP_CHAN_A);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
