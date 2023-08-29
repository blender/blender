/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BilateralBlurNode.h"
#include "COM_BilateralBlurOperation.h"

namespace blender::compositor {

BilateralBlurNode::BilateralBlurNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void BilateralBlurNode::convert_to_operations(NodeConverter &converter,
                                              const CompositorContext &context) const
{
  NodeBilateralBlurData *data = (NodeBilateralBlurData *)this->get_bnode()->storage;
  BilateralBlurOperation *operation = new BilateralBlurOperation();
  operation->set_quality(context.get_quality());
  operation->set_data(data);

  converter.add_operation(operation);
  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
