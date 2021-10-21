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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_SwitchNode.h"

namespace blender::compositor {

SwitchNode::SwitchNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SwitchNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext & /*context*/) const
{
  bool condition = this->get_bnode()->custom1;

  NodeOperationOutput *result;
  if (!condition) {
    result = converter.add_input_proxy(get_input_socket(0), false);
  }
  else {
    result = converter.add_input_proxy(get_input_socket(1), false);
  }

  converter.map_output_socket(get_output_socket(0), result);
}

}  // namespace blender::compositor
