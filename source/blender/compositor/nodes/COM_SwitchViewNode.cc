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
 * Copyright 2015, Blender Foundation.
 */

#include "COM_SwitchViewNode.h"

namespace blender::compositor {

SwitchViewNode::SwitchViewNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void SwitchViewNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext &context) const
{
  NodeOperationOutput *result;
  const char *view_name = context.get_view_name();
  bNode *bnode = this->get_bnode();

  /* get the internal index of the socket with a matching name */
  int nr = BLI_findstringindex(&bnode->inputs, view_name, offsetof(bNodeSocket, name));
  nr = MAX2(nr, 0);

  result = converter.add_input_proxy(get_input_socket(nr), false);
  converter.map_output_socket(get_output_socket(0), result);
}

}  // namespace blender::compositor
