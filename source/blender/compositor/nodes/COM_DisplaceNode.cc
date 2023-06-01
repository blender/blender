/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
