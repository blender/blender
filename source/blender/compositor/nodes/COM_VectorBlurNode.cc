/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_VectorBlurNode.h"
#include "COM_VectorBlurOperation.h"

namespace blender::compositor {

VectorBlurNode::VectorBlurNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void VectorBlurNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext &context) const
{
  const bNode *node = this->get_bnode();
  const NodeBlurData *vector_blur_settings = (const NodeBlurData *)node->storage;

  VectorBlurOperation *operation = new VectorBlurOperation();
  operation->set_vector_blur_settings(vector_blur_settings);
  operation->set_quality(context.get_quality());
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
  converter.map_output_socket(get_output_socket(), operation->get_output_socket());
}

}  // namespace blender::compositor
