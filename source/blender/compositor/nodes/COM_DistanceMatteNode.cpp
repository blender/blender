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

#include "COM_DistanceMatteNode.h"
#include "BKE_node.h"
#include "COM_ConvertOperation.h"
#include "COM_DistanceRGBMatteOperation.h"
#include "COM_DistanceYCCMatteOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

DistanceMatteNode::DistanceMatteNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void DistanceMatteNode::convertToOperations(NodeConverter &converter,
                                            const CompositorContext & /*context*/) const
{
  bNode *editorsnode = getbNode();
  NodeChroma *storage = (NodeChroma *)editorsnode->storage;

  NodeInput *inputSocketImage = this->getInputSocket(0);
  NodeInput *inputSocketKey = this->getInputSocket(1);
  NodeOutput *outputSocketImage = this->getOutputSocket(0);
  NodeOutput *outputSocketMatte = this->getOutputSocket(1);

  SetAlphaMultiplyOperation *operationAlpha = new SetAlphaMultiplyOperation();
  converter.addOperation(operationAlpha);

  /* work in RGB color space */
  NodeOperation *operation;
  if (storage->channel == 1) {
    DistanceRGBMatteOperation *matte = new DistanceRGBMatteOperation();
    matte->setSettings(storage);
    converter.addOperation(matte);

    converter.mapInputSocket(inputSocketImage, matte->getInputSocket(0));
    converter.mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));

    converter.mapInputSocket(inputSocketKey, matte->getInputSocket(1));

    operation = matte;
  }
  /* work in YCbCr color space */
  else {
    DistanceYCCMatteOperation *matte = new DistanceYCCMatteOperation();
    matte->setSettings(storage);
    converter.addOperation(matte);

    ConvertRGBToYCCOperation *operationYCCImage = new ConvertRGBToYCCOperation();
    ConvertRGBToYCCOperation *operationYCCMatte = new ConvertRGBToYCCOperation();
    operationYCCImage->setMode(BLI_YCC_ITU_BT709);
    operationYCCMatte->setMode(BLI_YCC_ITU_BT709);
    converter.addOperation(operationYCCImage);
    converter.addOperation(operationYCCMatte);

    converter.mapInputSocket(inputSocketImage, operationYCCImage->getInputSocket(0));
    converter.addLink(operationYCCImage->getOutputSocket(), matte->getInputSocket(0));
    converter.addLink(operationYCCImage->getOutputSocket(), operationAlpha->getInputSocket(0));

    converter.mapInputSocket(inputSocketKey, operationYCCMatte->getInputSocket(0));
    converter.addLink(operationYCCMatte->getOutputSocket(), matte->getInputSocket(1));

    operation = matte;
  }

  converter.mapOutputSocket(outputSocketMatte, operation->getOutputSocket(0));
  converter.addLink(operation->getOutputSocket(), operationAlpha->getInputSocket(1));

  if (storage->channel != 1) {
    ConvertYCCToRGBOperation *inv_convert = new ConvertYCCToRGBOperation();
    inv_convert->setMode(BLI_YCC_ITU_BT709);

    converter.addOperation(inv_convert);
    converter.addLink(operationAlpha->getOutputSocket(0), inv_convert->getInputSocket(0));
    converter.mapOutputSocket(outputSocketImage, inv_convert->getOutputSocket());
    converter.addPreview(inv_convert->getOutputSocket());
  }
  else {
    converter.mapOutputSocket(outputSocketImage, operationAlpha->getOutputSocket());
    converter.addPreview(operationAlpha->getOutputSocket());
  }
}
