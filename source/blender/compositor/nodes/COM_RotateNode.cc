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

RotateNode::RotateNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void RotateNode::convertToOperations(NodeConverter &converter,
                                     const CompositorContext &context) const
{
  NodeInput *inputSocket = this->getInputSocket(0);
  NodeInput *inputDegreeSocket = this->getInputSocket(1);
  NodeOutput *outputSocket = this->getOutputSocket(0);
  RotateOperation *operation = new RotateOperation();
  converter.addOperation(operation);

  PixelSampler sampler = (PixelSampler)this->getbNode()->custom1;
  switch (context.get_execution_model()) {
    case eExecutionModel::Tiled: {
      SetSamplerOperation *sampler_op = new SetSamplerOperation();
      sampler_op->setSampler(sampler);
      converter.addOperation(sampler_op);
      converter.addLink(sampler_op->getOutputSocket(), operation->getInputSocket(0));
      converter.mapInputSocket(inputSocket, sampler_op->getInputSocket(0));
      break;
    }
    case eExecutionModel::FullFrame: {
      operation->set_sampler(sampler);
      converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
      break;
    }
  }

  converter.mapInputSocket(inputDegreeSocket, operation->getInputSocket(1));
  converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
}

}  // namespace blender::compositor
