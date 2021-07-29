/*
 * Copyright 2011, Blender Foundation.
 *
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
 * Contributor:
 *		Jeroen Bakker
 *		Monique Dewanchand
 */

#include "COM_ScaleNode.h"

#include "COM_ScaleOperation.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"
#include "COM_SetValueOperation.h"
#include "COM_SetSamplerOperation.h"

ScaleNode::ScaleNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ScaleNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *bnode = this->getbNode();
	
	NodeInput *inputSocket = this->getInputSocket(0);
	NodeInput *inputXSocket = this->getInputSocket(1);
	NodeInput *inputYSocket = this->getInputSocket(2);
	NodeOutput *outputSocket = this->getOutputSocket(0);

	switch (bnode->custom1) {
		case CMP_SCALE_RELATIVE:
		{
			ScaleOperation *operation = new ScaleOperation();
			converter.addOperation(operation);
			
			converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
			converter.mapInputSocket(inputXSocket, operation->getInputSocket(1));
			converter.mapInputSocket(inputYSocket, operation->getInputSocket(2));
			converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
			break;
		}
		case CMP_SCALE_SCENEPERCENT:
		{
			SetValueOperation *scaleFactorOperation = new SetValueOperation();
			scaleFactorOperation->setValue(context.getRenderData()->size / 100.0f);
			converter.addOperation(scaleFactorOperation);
			
			ScaleOperation *operation = new ScaleOperation();
			converter.addOperation(operation);
			
			converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
			converter.addLink(scaleFactorOperation->getOutputSocket(), operation->getInputSocket(1));
			converter.addLink(scaleFactorOperation->getOutputSocket(), operation->getInputSocket(2));
			converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
			break;
		}
		case CMP_SCALE_RENDERPERCENT:
		{
			const RenderData *rd = context.getRenderData();
			ScaleFixedSizeOperation *operation = new ScaleFixedSizeOperation();
			/* framing options */
			operation->setIsAspect((bnode->custom2 & CMP_SCALE_RENDERSIZE_FRAME_ASPECT) != 0);
			operation->setIsCrop((bnode->custom2 & CMP_SCALE_RENDERSIZE_FRAME_CROP) != 0);
			operation->setOffset(bnode->custom3, bnode->custom4);
			operation->setNewWidth(rd->xsch * rd->size / 100.0f);
			operation->setNewHeight(rd->ysch * rd->size / 100.0f);
			operation->getInputSocket(0)->setResizeMode(COM_SC_NO_RESIZE);
			converter.addOperation(operation);
			
			converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
			converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
			break;
		}
		case CMP_SCALE_ABSOLUTE:
		{
			/* TODO: what is the use of this one.... perhaps some issues when the ui was updated... */
			ScaleAbsoluteOperation *operation = new ScaleAbsoluteOperation();
			converter.addOperation(operation);
			
			converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
			converter.mapInputSocket(inputXSocket, operation->getInputSocket(1));
			converter.mapInputSocket(inputYSocket, operation->getInputSocket(2));
			converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
			break;
		}
	}
}
