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

#include "COM_TimeNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"

#include "BKE_colortools.h"

#include "BLI_utildefines.h"

TimeNode::TimeNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void TimeNode::convertToOperations(NodeConverter &converter,
                                   const CompositorContext &context) const
{
  SetValueOperation *operation = new SetValueOperation();
  bNode *node = this->getbNode();

  /* stack order output: fac */
  float fac = 0.0f;
  const int framenumber = context.getFramenumber();

  if (framenumber < node->custom1) {
    fac = 0.0f;
  }
  else if (framenumber > node->custom2) {
    fac = 1.0f;
  }
  else if (node->custom1 < node->custom2) {
    fac = (context.getFramenumber() - node->custom1) / (float)(node->custom2 - node->custom1);
  }

  BKE_curvemapping_init((CurveMapping *)node->storage);
  fac = BKE_curvemapping_evaluateF((CurveMapping *)node->storage, 0, fac);
  operation->setValue(clamp_f(fac, 0.0f, 1.0f));
  converter.addOperation(operation);

  converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket());
}
