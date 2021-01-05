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
 * Copyright 2018, Blender Foundation.
 */

#include "COM_CryptomatteNode.h"
#include "COM_ConvertOperation.h"
#include "COM_CryptomatteOperation.h"

#include "BLI_assert.h"
#include "BLI_hash_mm3.h"
#include "BLI_listbase.h"
#include "BLI_string.h"

#include "COM_SetAlphaMultiplyOperation.h"
#include <iterator>

CryptomatteNode::CryptomatteNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void CryptomatteNode::convertToOperations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  NodeInput *inputSocketImage = this->getInputSocket(0);
  NodeOutput *outputSocketImage = this->getOutputSocket(0);
  NodeOutput *outputSocketMatte = this->getOutputSocket(1);
  NodeOutput *outputSocketPick = this->getOutputSocket(2);

  bNode *node = this->getbNode();
  NodeCryptomatte *cryptoMatteSettings = (NodeCryptomatte *)node->storage;

  CryptomatteOperation *operation = new CryptomatteOperation(getNumberOfInputSockets() - 1);
  if (cryptoMatteSettings) {
    LISTBASE_FOREACH (CryptomatteEntry *, cryptomatte_entry, &cryptoMatteSettings->entries) {
      operation->addObjectIndex(cryptomatte_entry->encoded_hash);
    }
  }

  converter.addOperation(operation);

  for (int i = 0; i < getNumberOfInputSockets() - 1; i++) {
    converter.mapInputSocket(this->getInputSocket(i + 1), operation->getInputSocket(i));
  }

  SeparateChannelOperation *separateOperation = new SeparateChannelOperation;
  separateOperation->setChannel(3);
  converter.addOperation(separateOperation);

  SetAlphaMultiplyOperation *operationAlpha = new SetAlphaMultiplyOperation();
  converter.addOperation(operationAlpha);

  converter.addLink(operation->getOutputSocket(0), separateOperation->getInputSocket(0));
  converter.addLink(separateOperation->getOutputSocket(0), operationAlpha->getInputSocket(1));

  SetAlphaMultiplyOperation *clearAlphaOperation = new SetAlphaMultiplyOperation();
  converter.addOperation(clearAlphaOperation);
  converter.addInputValue(clearAlphaOperation->getInputSocket(1), 1.0f);

  converter.addLink(operation->getOutputSocket(0), clearAlphaOperation->getInputSocket(0));

  converter.mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));
  converter.mapOutputSocket(outputSocketMatte, separateOperation->getOutputSocket(0));
  converter.mapOutputSocket(outputSocketImage, operationAlpha->getOutputSocket(0));
  converter.mapOutputSocket(outputSocketPick, clearAlphaOperation->getOutputSocket(0));
}
