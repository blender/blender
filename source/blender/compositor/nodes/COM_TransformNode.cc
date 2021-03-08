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

#include "COM_TransformNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetSamplerOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_TranslateOperation.h"

TransformNode::TransformNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void TransformNode::convertToOperations(NodeConverter &converter,
                                        const CompositorContext & /*context*/) const
{
  NodeInput *imageInput = this->getInputSocket(0);
  NodeInput *xInput = this->getInputSocket(1);
  NodeInput *yInput = this->getInputSocket(2);
  NodeInput *angleInput = this->getInputSocket(3);
  NodeInput *scaleInput = this->getInputSocket(4);

  ScaleOperation *scaleOperation = new ScaleOperation();
  converter.addOperation(scaleOperation);

  RotateOperation *rotateOperation = new RotateOperation();
  rotateOperation->setDoDegree2RadConversion(false);
  converter.addOperation(rotateOperation);

  TranslateOperation *translateOperation = new TranslateOperation();
  converter.addOperation(translateOperation);

  SetSamplerOperation *sampler = new SetSamplerOperation();
  sampler->setSampler((PixelSampler)this->getbNode()->custom1);
  converter.addOperation(sampler);

  converter.mapInputSocket(imageInput, sampler->getInputSocket(0));
  converter.addLink(sampler->getOutputSocket(), scaleOperation->getInputSocket(0));
  converter.mapInputSocket(scaleInput, scaleOperation->getInputSocket(1));
  converter.mapInputSocket(scaleInput, scaleOperation->getInputSocket(2));  // xscale = yscale

  converter.addLink(scaleOperation->getOutputSocket(), rotateOperation->getInputSocket(0));
  converter.mapInputSocket(angleInput, rotateOperation->getInputSocket(1));

  converter.addLink(rotateOperation->getOutputSocket(), translateOperation->getInputSocket(0));
  converter.mapInputSocket(xInput, translateOperation->getInputSocket(1));
  converter.mapInputSocket(yInput, translateOperation->getInputSocket(2));

  converter.mapOutputSocket(getOutputSocket(), translateOperation->getOutputSocket());
}
