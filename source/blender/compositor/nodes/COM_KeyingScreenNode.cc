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

#include "COM_KeyingScreenNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_KeyingScreenOperation.h"

#include "DNA_movieclip_types.h"

KeyingScreenNode::KeyingScreenNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void KeyingScreenNode::convertToOperations(NodeConverter &converter,
                                           const CompositorContext &context) const
{
  bNode *editorNode = this->getbNode();
  MovieClip *clip = (MovieClip *)editorNode->id;
  NodeKeyingScreenData *keyingscreen_data = (NodeKeyingScreenData *)editorNode->storage;

  NodeOutput *outputScreen = this->getOutputSocket(0);

  // always connect the output image
  KeyingScreenOperation *operation = new KeyingScreenOperation();
  operation->setMovieClip(clip);
  operation->setTrackingObject(keyingscreen_data->tracking_object);
  operation->setFramenumber(context.getFramenumber());
  converter.addOperation(operation);

  converter.mapOutputSocket(outputScreen, operation->getOutputSocket());
}
