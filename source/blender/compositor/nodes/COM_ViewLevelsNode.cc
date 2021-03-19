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

#include "COM_ViewLevelsNode.h"
#include "COM_CalculateMeanOperation.h"
#include "COM_CalculateStandardDeviationOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"

ViewLevelsNode::ViewLevelsNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void ViewLevelsNode::convertToOperations(NodeConverter &converter,
                                         const CompositorContext & /*context*/) const
{
  NodeInput *input = this->getInputSocket(0);
  if (input->isLinked()) {
    // add preview to inputSocket;

    /* calculate mean operation */
    {
      CalculateMeanOperation *operation = new CalculateMeanOperation();
      operation->setSetting(this->getbNode()->custom1);

      converter.addOperation(operation);
      converter.mapInputSocket(input, operation->getInputSocket(0));
      converter.mapOutputSocket(this->getOutputSocket(0), operation->getOutputSocket());
    }

    /* calculate standard deviation operation */
    {
      CalculateStandardDeviationOperation *operation = new CalculateStandardDeviationOperation();
      operation->setSetting(this->getbNode()->custom1);

      converter.addOperation(operation);
      converter.mapInputSocket(input, operation->getInputSocket(0));
      converter.mapOutputSocket(this->getOutputSocket(1), operation->getOutputSocket());
    }
  }
  else {
    converter.addOutputValue(getOutputSocket(0), 0.0f);
    converter.addOutputValue(getOutputSocket(1), 0.0f);
  }
}
