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

#include "COM_SetAlphaNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetAlphaMultiplyOperation.h"
#include "COM_SetAlphaReplaceOperation.h"

void SetAlphaNode::convertToOperations(NodeConverter &converter,
                                       const CompositorContext & /*context*/) const
{
  const bNode *editorNode = this->getbNode();
  const NodeSetAlpha *storage = static_cast<const NodeSetAlpha *>(editorNode->storage);
  NodeOperation *operation = nullptr;
  switch (storage->mode) {
    case CMP_NODE_SETALPHA_MODE_APPLY:
      operation = new SetAlphaMultiplyOperation();
      break;
    case CMP_NODE_SETALPHA_MODE_REPLACE_ALPHA:
      operation = new SetAlphaReplaceOperation();
      break;
  }

  if (!this->getInputSocket(0)->isLinked() && this->getInputSocket(1)->isLinked()) {
    operation->setResolutionInputSocketIndex(1);
  }

  converter.addOperation(operation);

  converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
  converter.mapInputSocket(getInputSocket(1), operation->getInputSocket(1));
  converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket());
}
