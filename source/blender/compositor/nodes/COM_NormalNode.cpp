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

#include "COM_NormalNode.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"
#include "COM_DotproductOperation.h"
#include "COM_SetVectorOperation.h"

NormalNode::NormalNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void NormalNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *inputSocket = this->getInputSocket(0);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	OutputSocket *outputSocketDotproduct = this->getOutputSocket(1);
	bNode *editorNode = this->getbNode();
	
	SetVectorOperation *operationSet = new SetVectorOperation();
	bNodeSocket *insock = (bNodeSocket *)editorNode->outputs.first;
	bNodeSocketValueVector *dval = (bNodeSocketValueVector *)insock->default_value;
	operationSet->setX(dval->value[0]);
	operationSet->setY(dval->value[1]);
	operationSet->setZ(dval->value[2]);
	operationSet->setW(0.0f);
	
	outputSocket->relinkConnections(operationSet->getOutputSocket(0));
	graph->addOperation(operationSet);
	
	if (outputSocketDotproduct->isConnected()) {
		DotproductOperation *operation = new DotproductOperation();
		outputSocketDotproduct->relinkConnections(operation->getOutputSocket(0));
		inputSocket->relinkConnections(operation->getInputSocket(0), 0, graph);
		addLink(graph, operationSet->getOutputSocket(0), operation->getInputSocket(1));
		graph->addOperation(operation);
	}
}
