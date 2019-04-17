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

#include "COM_TextureNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_TextureOperation.h"

TextureNode::TextureNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void TextureNode::convertToOperations(NodeConverter &converter,
                                      const CompositorContext &context) const
{
  bNode *editorNode = this->getbNode();
  Tex *texture = (Tex *)editorNode->id;
  TextureOperation *operation = new TextureOperation();
  const ColorManagedDisplaySettings *displaySettings = context.getDisplaySettings();
  bool sceneColorManage = !STREQ(displaySettings->display_device, "None");
  operation->setTexture(texture);
  operation->setRenderData(context.getRenderData());
  operation->setSceneColorManage(sceneColorManage);
  converter.addOperation(operation);

  converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
  converter.mapInputSocket(getInputSocket(1), operation->getInputSocket(1));
  converter.mapOutputSocket(getOutputSocket(1), operation->getOutputSocket());

  converter.addPreview(operation->getOutputSocket());

  TextureAlphaOperation *alphaOperation = new TextureAlphaOperation();
  alphaOperation->setTexture(texture);
  alphaOperation->setRenderData(context.getRenderData());
  alphaOperation->setSceneColorManage(sceneColorManage);
  converter.addOperation(alphaOperation);

  converter.mapInputSocket(getInputSocket(0), alphaOperation->getInputSocket(0));
  converter.mapInputSocket(getInputSocket(1), alphaOperation->getInputSocket(1));
  converter.mapOutputSocket(getOutputSocket(0), alphaOperation->getOutputSocket());
}
