/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_FlipNode.h"

#include "COM_FlipOperation.h"

namespace blender::compositor {

FlipNode::FlipNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void FlipNode::convert_to_operations(NodeConverter &converter,
                                     const CompositorContext & /*context*/) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeOutput *output_socket = this->get_output_socket(0);
  FlipOperation *operation = new FlipOperation();
  switch (this->get_bnode()->custom1) {
    case 0: /* TODO: I didn't find any constants in the old implementation,
             * should I introduce them. */
      operation->setFlipX(true);
      operation->setFlipY(false);
      break;
    case 1:
      operation->setFlipX(false);
      operation->setFlipY(true);
      break;
    case 2:
      operation->setFlipX(true);
      operation->setFlipY(true);
      break;
  }

  converter.add_operation(operation);
  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
}

}  // namespace blender::compositor
