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

#include "COM_EllipseMaskNode.h"
#include "COM_EllipseMaskOperation.h"
#include "COM_ExecutionSystem.h"

#include "COM_SetValueOperation.h"
#include "COM_ScaleOperation.h"

EllipseMaskNode::EllipseMaskNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void EllipseMaskNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	NodeInput *inputSocket = this->getInputSocket(0);
	NodeOutput *outputSocket = this->getOutputSocket(0);
	
	EllipseMaskOperation *operation;
	operation = new EllipseMaskOperation();
	operation->setData((NodeEllipseMask *)this->getbNode()->storage);
	operation->setMaskType(this->getbNode()->custom1);
	converter.addOperation(operation);
	
	if (inputSocket->isLinked()) {
		converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
		converter.mapOutputSocket(outputSocket, operation->getOutputSocket());
	}
	else {
		/* Value operation to produce original transparent image */
		SetValueOperation *valueOperation = new SetValueOperation();
		valueOperation->setValue(0.0f);
		converter.addOperation(valueOperation);

		/* Scale that image up to render resolution */
		const RenderData *rd = context.getRenderData();
		ScaleFixedSizeOperation *scaleOperation = new ScaleFixedSizeOperation();
		scaleOperation->setIsAspect(false);
		scaleOperation->setIsCrop(false);
		scaleOperation->setOffset(0.0f, 0.0f);
		scaleOperation->setNewWidth(rd->xsch * rd->size / 100.0f);
		scaleOperation->setNewHeight(rd->ysch * rd->size / 100.0f);
		scaleOperation->getInputSocket(0)->setResizeMode(COM_SC_NO_RESIZE);
		converter.addOperation(scaleOperation);

		converter.addLink(valueOperation->getOutputSocket(0), scaleOperation->getInputSocket(0));
		converter.addLink(scaleOperation->getOutputSocket(0), operation->getInputSocket(0));
		converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
	}
	
	converter.mapInputSocket(getInputSocket(1), operation->getInputSocket(1));
}
