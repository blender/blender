/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2012, Blender Foundation.
 */

#include "COM_MapRangeNode.h"

#include "COM_MapRangeOperation.h"

namespace blender::compositor {

MapRangeNode::MapRangeNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void MapRangeNode::convert_to_operations(NodeConverter &converter,
                                         const CompositorContext & /*context*/) const
{
  NodeInput *value_socket = this->get_input_socket(0);
  NodeInput *source_min_socket = this->get_input_socket(1);
  NodeInput *source_max_socket = this->get_input_socket(2);
  NodeInput *dest_min_socket = this->get_input_socket(3);
  NodeInput *dest_max_socket = this->get_input_socket(4);
  NodeOutput *output_socket = this->get_output_socket(0);

  MapRangeOperation *operation = new MapRangeOperation();
  operation->set_use_clamp(this->get_bnode()->custom1);
  converter.add_operation(operation);

  converter.map_input_socket(value_socket, operation->get_input_socket(0));
  converter.map_input_socket(source_min_socket, operation->get_input_socket(1));
  converter.map_input_socket(source_max_socket, operation->get_input_socket(2));
  converter.map_input_socket(dest_min_socket, operation->get_input_socket(3));
  converter.map_input_socket(dest_max_socket, operation->get_input_socket(4));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
}

}  // namespace blender::compositor
