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

#include "COM_ConvertAlphaNode.h"
#include "COM_ConvertOperation.h"
#include "COM_ExecutionSystem.h"

void ConvertAlphaNode::convertToOperations(NodeConverter &converter,
                                           const CompositorContext & /*context*/) const
{
  NodeOperation *operation = nullptr;
  bNode *node = this->getbNode();

  /* value hardcoded in rna_nodetree.c */
  if (node->custom1 == 1) {
    operation = new ConvertPremulToStraightOperation();
  }
  else {
    operation = new ConvertStraightToPremulOperation();
  }

  converter.addOperation(operation);

  converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
  converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket());
}
