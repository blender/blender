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

#include "COM_ColorRampNode.h"
#include "BKE_node.h"
#include "COM_ColorRampOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_ExecutionSystem.h"
#include "DNA_texture_types.h"

ColorRampNode::ColorRampNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void ColorRampNode::convertToOperations(NodeConverter &converter,
                                        const CompositorContext & /*context*/) const
{
  NodeInput *inputSocket = this->getInputSocket(0);
  NodeOutput *outputSocket = this->getOutputSocket(0);
  NodeOutput *outputSocketAlpha = this->getOutputSocket(1);
  bNode *editorNode = this->getbNode();

  ColorRampOperation *operation = new ColorRampOperation();
  operation->setColorBand((ColorBand *)editorNode->storage);
  converter.addOperation(operation);

  converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
  converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));

  SeparateChannelOperation *operation2 = new SeparateChannelOperation();
  operation2->setChannel(3);
  converter.addOperation(operation2);

  converter.addLink(operation->getOutputSocket(), operation2->getInputSocket(0));
  converter.mapOutputSocket(outputSocketAlpha, operation2->getOutputSocket());
}
