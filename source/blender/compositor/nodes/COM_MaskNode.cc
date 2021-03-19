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

#include "COM_MaskNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_MaskOperation.h"

#include "DNA_mask_types.h"

MaskNode::MaskNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void MaskNode::convertToOperations(NodeConverter &converter,
                                   const CompositorContext &context) const
{
  const RenderData *rd = context.getRenderData();
  const float render_size_factor = context.getRenderPercentageAsFactor();

  NodeOutput *outputMask = this->getOutputSocket(0);

  bNode *editorNode = this->getbNode();
  NodeMask *data = (NodeMask *)editorNode->storage;
  Mask *mask = (Mask *)editorNode->id;

  // always connect the output image
  MaskOperation *operation = new MaskOperation();

  if (editorNode->custom1 & CMP_NODEFLAG_MASK_FIXED) {
    operation->setMaskWidth(data->size_x);
    operation->setMaskHeight(data->size_y);
  }
  else if (editorNode->custom1 & CMP_NODEFLAG_MASK_FIXED_SCENE) {
    operation->setMaskWidth(data->size_x * render_size_factor);
    operation->setMaskHeight(data->size_y * render_size_factor);
  }
  else {
    operation->setMaskWidth(rd->xsch * render_size_factor);
    operation->setMaskHeight(rd->ysch * render_size_factor);
  }

  operation->setMask(mask);
  operation->setFramenumber(context.getFramenumber());
  operation->setFeather((bool)(editorNode->custom1 & CMP_NODEFLAG_MASK_NO_FEATHER) == 0);

  if ((editorNode->custom1 & CMP_NODEFLAG_MASK_MOTION_BLUR) && (editorNode->custom2 > 1) &&
      (editorNode->custom3 > FLT_EPSILON)) {
    operation->setMotionBlurSamples(editorNode->custom2);
    operation->setMotionBlurShutter(editorNode->custom3);
  }

  converter.addOperation(operation);
  converter.mapOutputSocket(outputMask, operation->getOutputSocket());
}
