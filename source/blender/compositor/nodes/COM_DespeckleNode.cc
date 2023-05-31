/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DespeckleNode.h"
#include "COM_DespeckleOperation.h"

namespace blender::compositor {

DespeckleNode::DespeckleNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DespeckleNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  const bNode *editor_node = this->get_bnode();
  NodeInput *input_socket = this->get_input_socket(0);
  NodeInput *input_image_socket = this->get_input_socket(1);
  NodeOutput *output_socket = this->get_output_socket(0);

  DespeckleOperation *operation = new DespeckleOperation();
  operation->set_threshold(editor_node->custom3);
  operation->set_threshold_neighbor(editor_node->custom4);
  converter.add_operation(operation);

  converter.map_input_socket(input_image_socket, operation->get_input_socket(0));
  converter.map_input_socket(input_socket, operation->get_input_socket(1));
  converter.map_output_socket(output_socket, operation->get_output_socket());

  converter.add_preview(operation->get_output_socket(0));
}

}  // namespace blender::compositor
