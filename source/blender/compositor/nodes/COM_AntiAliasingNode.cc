/*
 * Copyright 2017, Blender Foundation.
 *
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
 * Contributor: IRIE Shinsuke
 */

#include "COM_AntiAliasingNode.h"
#include "COM_SMAAOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

void AntiAliasingNode::convertToOperations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  bNode *node = this->getbNode();
  NodeAntiAliasingData *data = (NodeAntiAliasingData *)node->storage;

  /* Edge Detection (First Pass) */
  SMAAEdgeDetectionOperation *operation1 = nullptr;

  operation1 = new SMAAEdgeDetectionOperation();
  operation1->setThreshold(data->threshold);
  operation1->setLocalContrastAdaptationFactor(data->contrast_limit);
  converter.addOperation(operation1);

  converter.mapInputSocket(getInputSocket(0), operation1->getInputSocket(0));

  /* Blending Weight Calculation Pixel Shader (Second Pass) */
  SMAABlendingWeightCalculationOperation *operation2 =
      new SMAABlendingWeightCalculationOperation();
  operation2->setCornerRounding(data->corner_rounding);
  converter.addOperation(operation2);

  converter.addLink(operation1->getOutputSocket(), operation2->getInputSocket(0));

  /* Neighborhood Blending Pixel Shader (Third Pass) */
  SMAANeighborhoodBlendingOperation *operation3 = new SMAANeighborhoodBlendingOperation();
  converter.addOperation(operation3);

  converter.mapInputSocket(getInputSocket(0), operation3->getInputSocket(0));
  converter.addLink(operation2->getOutputSocket(), operation3->getInputSocket(1));
  converter.mapOutputSocket(getOutputSocket(0), operation3->getOutputSocket());
}

}  // namespace blender::compositor
