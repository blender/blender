/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DirectionalBlurNode.h"
#include "COM_DirectionalBlurOperation.h"

namespace blender::compositor {

DirectionalBlurNode::DirectionalBlurNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DirectionalBlurNode::convert_to_operations(NodeConverter &converter,
                                                const CompositorContext &context) const
{
  const NodeDBlurData *data = (const NodeDBlurData *)this->get_bnode()->storage;
  DirectionalBlurOperation *operation = new DirectionalBlurOperation();
  operation->set_quality(context.get_quality());
  operation->set_data(data);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket());
}

}  // namespace blender::compositor
