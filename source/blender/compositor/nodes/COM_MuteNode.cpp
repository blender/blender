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

#include "COM_MuteNode.h"
#include "COM_SocketConnection.h"
#include "stdio.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"

MuteNode::MuteNode(bNode *editorNode): Node(editorNode)
{
}

void MuteNode::reconnect(ExecutionSystem * graph, OutputSocket * output)
{
	vector<InputSocket*> &inputsockets = this->getInputSockets();
	for (unsigned int index = 0; index < inputsockets.size() ; index ++) {
		InputSocket *input = inputsockets[index];
		if (input->getDataType() == output->getDataType()) {
			if (input->isConnected()) {
				output->relinkConnections(input->getConnection()->getFromSocket(), false);
				return;
			}
		}
	}
	
	NodeOperation * operation;
	switch (output->getDataType()) {
	case COM_DT_VALUE:
	{
		SetValueOperation *valueoperation = new SetValueOperation();
		valueoperation->setValue(0.0f);
		operation = valueoperation;
		break;
	}
	case COM_DT_VECTOR:
	{
		SetVectorOperation *vectoroperation = new SetVectorOperation();
		vectoroperation->setX(0.0f);
		vectoroperation->setY(0.0f);
		vectoroperation->setW(0.0f);
		operation = vectoroperation;
		break;
	}
	case COM_DT_COLOR:
	{
		SetColorOperation *coloroperation = new SetColorOperation();
		coloroperation->setChannel1(0.0f);
		coloroperation->setChannel2(0.0f);
		coloroperation->setChannel3(0.0f);
		coloroperation->setChannel4(0.0f);
		operation = coloroperation;
		break;
	}
		/* quiet warnings */
	case COM_DT_UNKNOWN:
		break;
	}

	if (operation) {
		output->relinkConnections(operation->getOutputSocket(), false);
		graph->addOperation(operation);
	}

	output->clearConnections();
}

void MuteNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	vector<OutputSocket*> &outputsockets = this->getOutputSockets();

	for (unsigned int index = 0 ; index < outputsockets.size() ; index ++) {
		OutputSocket * output = outputsockets[index];
		if (output->isConnected()) {
			reconnect(graph, output);
		}
	}
}
