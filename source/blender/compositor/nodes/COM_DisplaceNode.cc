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

#include "COM_DisplaceNode.h"
#include "COM_DisplaceOperation.h"
#include "COM_DisplaceSimpleOperation.h"

namespace blender::compositor {

DisplaceNode::DisplaceNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DisplaceNode::convert_to_operations(NodeConverter &converter,
                                         const CompositorContext &context) const
{
  NodeOperation *operation;
  if (context.get_quality() == eCompositorQuality::Low) {
    operation = new DisplaceSimpleOperation();
  }
  else {
    operation = new DisplaceOperation();
  }
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
  converter.map_input_socket(get_input_socket(3), operation->get_input_socket(3));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
}

}  // namespace blender::compositor
