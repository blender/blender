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

#include "COM_CombineRGBANode.h"

#include "COM_CombineChannelsOperation.h"

#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "DNA_material_types.h" // the ramp types


CombineRGBANode::CombineRGBANode(bNode *editorNode): Node(editorNode)
{
}


void CombineRGBANode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	InputSocket *inputRSocket = this->getInputSocket(0);
	InputSocket *inputGSocket = this->getInputSocket(1);
	InputSocket *inputBSocket = this->getInputSocket(2);
	InputSocket *inputASocket = this->getInputSocket(3);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	
	CombineChannelsOperation *operation = new CombineChannelsOperation();
	if (inputRSocket->isConnected()) {
		operation->setResolutionInputSocketIndex(0);
	}
	else if (inputGSocket->isConnected()) {
		operation->setResolutionInputSocketIndex(1);
	}
	else if (inputBSocket->isConnected()) {
		operation->setResolutionInputSocketIndex(2);
	}
	else {
		operation->setResolutionInputSocketIndex(3);
	}
	inputRSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	inputGSocket->relinkConnections(operation->getInputSocket(1), true, 1, graph);
	inputBSocket->relinkConnections(operation->getInputSocket(2), true, 2, graph);
	inputASocket->relinkConnections(operation->getInputSocket(3), true, 3, graph);
	outputSocket->relinkConnections(operation->getOutputSocket(0));
	graph->addOperation(operation);
}
