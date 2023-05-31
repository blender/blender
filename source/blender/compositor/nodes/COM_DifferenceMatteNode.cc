/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DifferenceMatteNode.h"
#include "COM_DifferenceMatteOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

namespace blender::compositor {

DifferenceMatteNode::DifferenceMatteNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DifferenceMatteNode::convert_to_operations(NodeConverter &converter,
                                                const CompositorContext & /*context*/) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeInput *input_socket2 = this->get_input_socket(1);
  NodeOutput *output_socket_image = this->get_output_socket(0);
  NodeOutput *output_socket_matte = this->get_output_socket(1);
  const bNode *editor_node = this->get_bnode();

  DifferenceMatteOperation *operation_set = new DifferenceMatteOperation();
  operation_set->set_settings((NodeChroma *)editor_node->storage);
  converter.add_operation(operation_set);

  converter.map_input_socket(input_socket, operation_set->get_input_socket(0));
  converter.map_input_socket(input_socket2, operation_set->get_input_socket(1));
  converter.map_output_socket(output_socket_matte, operation_set->get_output_socket(0));

  SetAlphaMultiplyOperation *operation = new SetAlphaMultiplyOperation();
  converter.add_operation(operation);

  converter.map_input_socket(input_socket, operation->get_input_socket(0));
  converter.add_link(operation_set->get_output_socket(), operation->get_input_socket(1));
  converter.map_output_socket(output_socket_image, operation->get_output_socket());

  converter.add_preview(operation->get_output_socket());
}

}  // namespace blender::compositor
