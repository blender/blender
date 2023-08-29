/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_RotateNode.h"

#include "COM_RotateOperation.h"
#include "COM_SetSamplerOperation.h"

namespace blender::compositor {

RotateNode::RotateNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void RotateNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext &context) const
{
  NodeInput *input_socket = this->get_input_socket(0);
  NodeInput *input_degree_socket = this->get_input_socket(1);
  NodeOutput *output_socket = this->get_output_socket(0);
  RotateOperation *operation = new RotateOperation();
  converter.add_operation(operation);

  PixelSampler sampler = (PixelSampler)this->get_bnode()->custom1;
  switch (context.get_execution_model()) {
    case eExecutionModel::Tiled: {
      SetSamplerOperation *sampler_op = new SetSamplerOperation();
      sampler_op->set_sampler(sampler);
      converter.add_operation(sampler_op);
      converter.add_link(sampler_op->get_output_socket(), operation->get_input_socket(0));
      converter.map_input_socket(input_socket, sampler_op->get_input_socket(0));
      break;
    }
    case eExecutionModel::FullFrame: {
      operation->set_sampler(sampler);
      converter.map_input_socket(input_socket, operation->get_input_socket(0));
      break;
    }
  }

  converter.map_input_socket(input_degree_socket, operation->get_input_socket(1));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
}

}  // namespace blender::compositor
