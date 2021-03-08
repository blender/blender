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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_ChannelMatteNode.h"
#include "BKE_node.h"
#include "COM_ChannelMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaMultiplyOperation.h"

ChannelMatteNode::ChannelMatteNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void ChannelMatteNode::convertToOperations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  bNode *node = this->getbNode();

  NodeInput *inputSocketImage = this->getInputSocket(0);
  NodeOutput *outputSocketImage = this->getOutputSocket(0);
  NodeOutput *outputSocketMatte = this->getOutputSocket(1);

  NodeOperation *convert = nullptr, *inv_convert = nullptr;
  /* colorspace */
  switch (node->custom1) {
    case CMP_NODE_CHANNEL_MATTE_CS_RGB:
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_HSV: /* HSV */
      convert = new ConvertRGBToHSVOperation();
      inv_convert = new ConvertHSVToRGBOperation();
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_YUV: /* YUV */
      convert = new ConvertRGBToYUVOperation();
      inv_convert = new ConvertYUVToRGBOperation();
      break;
    case CMP_NODE_CHANNEL_MATTE_CS_YCC: /* YCC */
      convert = new ConvertRGBToYCCOperation();
      ((ConvertRGBToYCCOperation *)convert)->setMode(BLI_YCC_ITU_BT709);
      inv_convert = new ConvertYCCToRGBOperation();
      ((ConvertYCCToRGBOperation *)inv_convert)->setMode(BLI_YCC_ITU_BT709);
      break;
    default:
      break;
  }

  ChannelMatteOperation *operation = new ChannelMatteOperation();
  /* pass the ui properties to the operation */
  operation->setSettings((NodeChroma *)node->storage, node->custom2);
  converter.addOperation(operation);

  SetAlphaMultiplyOperation *operationAlpha = new SetAlphaMultiplyOperation();
  converter.addOperation(operationAlpha);

  if (convert != nullptr) {
    converter.addOperation(convert);

    converter.mapInputSocket(inputSocketImage, convert->getInputSocket(0));
    converter.addLink(convert->getOutputSocket(), operation->getInputSocket(0));
    converter.addLink(convert->getOutputSocket(), operationAlpha->getInputSocket(0));
  }
  else {
    converter.mapInputSocket(inputSocketImage, operation->getInputSocket(0));
    converter.mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));
  }

  converter.mapOutputSocket(outputSocketMatte, operation->getOutputSocket(0));
  converter.addLink(operation->getOutputSocket(), operationAlpha->getInputSocket(1));

  if (inv_convert != nullptr) {
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
