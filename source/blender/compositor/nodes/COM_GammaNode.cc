/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GammaNode.h"
#include "COM_GammaOperation.h"

namespace blender::compositor {

GammaNode::GammaNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void GammaNode::convert_to_operations(NodeConverter &converter,
                                      const CompositorContext & /*context*/) const
{
  GammaOperation *operation = new GammaOperation();
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
