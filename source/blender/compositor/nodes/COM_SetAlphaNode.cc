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

#include "COM_SetAlphaNode.h"
#include "COM_SetAlphaMultiplyOperation.h"
#include "COM_SetAlphaReplaceOperation.h"

namespace blender::compositor {

void SetAlphaNode::convert_to_operations(NodeConverter &converter,
                                         const CompositorContext & /*context*/) const
{
  const bNode *editor_node = this->get_bnode();
  const NodeSetAlpha *storage = static_cast<const NodeSetAlpha *>(editor_node->storage);
  NodeOperation *operation = nullptr;
  switch (storage->mode) {
    case CMP_NODE_SETALPHA_MODE_APPLY:
      operation = new SetAlphaMultiplyOperation();
      break;
    case CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA:
      operation = new SetAlphaReplaceOperation();
      break;
  }

  if (!this->get_input_socket(0)->is_linked() && this->get_input_socket(1)->is_linked()) {
    operation->set_canvas_input_index(1);
  }

  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
}

}  // namespace blender::compositor
