/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_SplitNode.h"

#include "COM_SplitOperation.h"

namespace blender::compositor {

SplitNode::SplitNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SplitNode::convert_to_operations(NodeConverter &converter,
                                      const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();

  SplitOperation *split_operation = new SplitOperation();
  split_operation->set_split_percentage(node->custom1);
  split_operation->set_xsplit(node->custom2 == CMP_NODE_SPLIT_HORIZONTAL);

  converter.add_operation(split_operation);
  converter.map_input_socket(get_input_socket(0), split_operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), split_operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), split_operation->get_output_socket(0));

  converter.add_preview(split_operation->get_output_socket());
}

}  // namespace blender::compositor
