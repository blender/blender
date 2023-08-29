/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MapUVNode.h"
#include "COM_MapUVOperation.h"

namespace blender::compositor {

MapUVNode::MapUVNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void MapUVNode::convert_to_operations(NodeConverter &converter,
                                      const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();

  MapUVOperation *operation = new MapUVOperation();
  operation->set_alpha(float(node->custom1));
  operation->set_canvas_input_index(1);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
}

}  // namespace blender::compositor
