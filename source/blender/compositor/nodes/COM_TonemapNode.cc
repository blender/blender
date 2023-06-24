/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  const NodeTonemap *data = (const NodeTonemap *)this->get_bnode()->storage;

  TonemapOperation *operation = data->type == 1 ? new PhotoreceptorTonemapOperation() :
                                                  new TonemapOperation();
  operation->set_data(data);
  converter.add_operation(operation);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
