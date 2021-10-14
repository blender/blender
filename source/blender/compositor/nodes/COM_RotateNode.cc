/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

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
