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

#include "COM_TonemapNode.h"
#include "COM_TonemapOperation.h"

namespace blender::compositor {

TonemapNode::TonemapNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void TonemapNode::convert_to_operations(NodeConverter &converter,
                                        const CompositorContext & /*context*/) const
{
  NodeTonemap *data = (NodeTonemap *)this->get_bnode()->storage;

  TonemapOperation *operation = data->type == 1 ? new PhotoreceptorTonemapOperation() :
                                                  new TonemapOperation();
  operation->set_data(data);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
