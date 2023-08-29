/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DoubleEdgeMaskNode.h"
#include "COM_DoubleEdgeMaskOperation.h"

namespace blender::compositor {

DoubleEdgeMaskNode::DoubleEdgeMaskNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DoubleEdgeMaskNode::convert_to_operations(NodeConverter &converter,
                                               const CompositorContext & /*context*/) const
{
  DoubleEdgeMaskOperation *operation;
  const bNode *bnode = this->get_bnode();

  operation = new DoubleEdgeMaskOperation();
  operation->set_adjecent_only(bnode->custom1);
  operation->set_keep_inside(bnode->custom2);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
