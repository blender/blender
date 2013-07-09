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

#include "COM_BoxMaskNode.h"
#include "COM_BoxMaskOperation.h"
#include "COM_ExecutionSystem.h"

#include "COM_SetValueOperation.h"
#include "COM_ScaleOperation.h"

BoxMaskNode::BoxMaskNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void BoxMaskNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	BoxMaskOperation *operation;
	operation = new BoxMaskOperation();
	operation->setData((NodeBoxMask *)this->getbNode()->storage);

	InputSocket *inputSocket = this->getInputSocket(0);
	OutputSocket *outputSocket = this->getOutputSocket(0);

	if (inputSocket->isConnected()) {
		inputSocket->relinkConnections(operation->getInputSocket(0), 0, graph);
		outputSocket->relinkConnections(operation->getOutputSocket());
	}
	else {
		/* Value operation to produce original transparent image */
		SetValueOperation *valueOperation = new SetValueOperation();
		valueOperation->setValue(0.0f);
		graph->addOperation(valueOperation);

		/* Scale that image up to render resolution */
		const RenderData *rd = context->getRenderData();
		ScaleFixedSizeOperation *scaleOperation = new ScaleFixedSizeOperation();

		scaleOperation->setIsAspect(false);
		scaleOperation->setIsCrop(false);
		scaleOperation->setOffset(0.0f, 0.0f);

		scaleOperation->setNewWidth(rd->xsch * rd->size / 100.0f);
		scaleOperation->setNewHeight(rd->ysch * rd->size / 100.0f);

		addLink(graph, valueOperation->getOutputSocket(0), scaleOperation->getInputSocket(0));
		addLink(graph, scaleOperation->getOutputSocket(0), operation->getInputSocket(0));
		outputSocket->relinkConnections(operation->getOutputSocket(0));

		scaleOperation->getInputSocket(0)->getConnection()->setIgnoreResizeCheck(true);

		graph->addOperation(scaleOperation);
	}

	this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), 1, graph);
	operation->setMaskType(this->getbNode()->custom1);
	
	graph->addOperation(operation);
}
