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

#include "COM_IDMaskNode.h"
#include "COM_IDMaskOperation.h"
#include "COM_SMAAOperation.h"

namespace blender::compositor {

IDMaskNode::IDMaskNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}
void IDMaskNode::convertToOperations(NodeConverter &converter,
                                     const CompositorContext & /*context*/) const
{
  bNode *bnode = this->getbNode();

  IDMaskOperation *operation;
  operation = new IDMaskOperation();
  operation->setObjectIndex(bnode->custom1);
  converter.addOperation(operation);

  converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
  if (bnode->custom2 == 0) {
    converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
  }
  else {
    SMAAEdgeDetectionOperation *operation1 = nullptr;

    operation1 = new SMAAEdgeDetectionOperation();
    converter.addOperation(operation1);

    converter.addLink(operation->getOutputSocket(0), operation1->getInputSocket(0));

    /* Blending Weight Calculation Pixel Shader (Second Pass). */
    SMAABlendingWeightCalculationOperation *operation2 =
        new SMAABlendingWeightCalculationOperation();
    converter.addOperation(operation2);

    converter.addLink(operation1->getOutputSocket(), operation2->getInputSocket(0));

    /* Neighborhood Blending Pixel Shader (Third Pass). */
    SMAANeighborhoodBlendingOperation *operation3 = new SMAANeighborhoodBlendingOperation();
    converter.addOperation(operation3);

    converter.addLink(operation->getOutputSocket(0), operation3->getInputSocket(0));
    converter.addLink(operation2->getOutputSocket(), operation3->getInputSocket(1));
    converter.mapOutputSocket(getOutputSocket(0), operation3->getOutputSocket());
  }
}

}  // namespace blender::compositor
