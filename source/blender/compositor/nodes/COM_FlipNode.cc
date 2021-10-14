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

#include "COM_FlipNode.h"

#include "COM_FlipOperation.h"

namespace blender::compositor {

FlipNode::FlipNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void FlipNode::convert_to_operations(NodeConverter &converter,
                                     const CompositorContext & /*context*/) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);
  FlipOperation *operation = new FlipOperation();
  switch (this->get_bnode()->custom1) {
    case 0: /* TODO: I didn't find any constants in the old implementation,
             * should I introduce them. */
      operation->setFlipX(true);
      operation->setFlipY(false);
      break;
    case 1:
      operation->setFlipX(false);
      operation->setFlipY(true);
      break;
    case 2:
      operation->setFlipX(true);
      operation->setFlipY(true);
      break;
  }

  converter.add_operation(operation);
  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
}

}  // namespace blender::compositor
