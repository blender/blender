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

#include "COM_BokehImageNode.h"
#include "COM_BokehImageOperation.h"

namespace blender::compositor {

BokehImageNode::BokehImageNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void BokehImageNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  BokehImageOperation *operation = new BokehImageOperation();
  operation->set_data((NodeBokehImage *)this->get_bnode()->storage);

  converter.add_operation(operation);
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));

  converter.add_preview(operation->get_output_socket(0));
}

}  // namespace blender::compositor
