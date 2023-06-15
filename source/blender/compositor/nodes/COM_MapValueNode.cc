/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MapValueNode.h"

#include "COM_MapValueOperation.h"

namespace blender::compositor {

MapValueNode::MapValueNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void MapValueNode::convert_to_operations(NodeConverter &converter,
                                         const CompositorContext & /*context*/) const
{
  const TexMapping *storage = (const TexMapping *)this->get_bnode()->storage;

  NodeInput *color_socket = this->get_input_socket(0);
  NodeOutput *value_socket = this->get_output_socket(0);

  MapValueOperation *convert_prog = new MapValueOperation();
  convert_prog->set_settings(storage);
  converter.add_operation(convert_prog);

  converter.map_input_socket(color_socket, convert_prog->get_input_socket(0));
  converter.map_output_socket(value_socket, convert_prog->get_output_socket(0));
}

}  // namespace blender::compositor
