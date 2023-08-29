/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
