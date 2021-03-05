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
 * Copyright 2015, Blender Foundation.
 */

#include "COM_SwitchViewNode.h"
#include "BLI_listbase.h"

SwitchViewNode::SwitchViewNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void SwitchViewNode::convertToOperations(NodeConverter &converter,
                                         const CompositorContext &context) const
{
  NodeOperationOutput *result;
  const char *viewName = context.getViewName();
  bNode *bnode = this->getbNode();

  /* get the internal index of the socket with a matching name */
  int nr = BLI_findstringindex(&bnode->inputs, viewName, offsetof(bNodeSocket, name));
  nr = MAX2(nr, 0);

  result = converter.addInputProxy(getInputSocket(nr), false);
  converter.mapOutputSocket(getOutputSocket(0), result);
}
