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

#include "COM_SocketProxyNode.h"
#include "COM_SocketConnection.h"
#include "COM_SocketProxyOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"

SocketProxyNode::SocketProxyNode(bNode *editorNode, bNodeSocket *editorInput, bNodeSocket *editorOutput, bool buffer) : Node(editorNode, false)
{
	DataType dt;
	this->m_buffer = buffer;

	dt = COM_DT_VALUE;
	if (editorInput->type == SOCK_RGBA) dt = COM_DT_COLOR;
	if (editorInput->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
	this->addInputSocket(dt, (InputSocketResizeMode)editorInput->resizemode, editorInput);

	dt = COM_DT_VALUE;
	if (editorOutput->type == SOCK_RGBA) dt = COM_DT_COLOR;
	if (editorOutput->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
	this->addOutputSocket(dt, editorOutput);
}

void SocketProxyNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	OutputSocket *outputsocket = this->getOutputSocket(0);
	InputSocket *inputsocket = this->getInputSocket(0);
	if (inputsocket->isConnected()) {
		SocketProxyOperation *operation = new SocketProxyOperation(this->getOutputSocket()->getDataType());
		inputsocket->relinkConnections(operation->getInputSocket(0));
		outputsocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
		
		if (m_buffer) {
			WriteBufferOperation *writeOperation = new WriteBufferOperation();
			ReadBufferOperation *readOperation = new ReadBufferOperation();
			readOperation->setMemoryProxy(writeOperation->getMemoryProxy());
			
			operation->getOutputSocket()->relinkConnections(readOperation->getOutputSocket());
			addLink(graph, operation->getOutputSocket(), writeOperation->getInputSocket(0));
			
			graph->addOperation(writeOperation);
			graph->addOperation(readOperation);
		}
	}
	else if (outputsocket->isConnected()) {
		/* If input is not connected, add a constant value operation instead */
		switch (outputsocket->getDataType()) {
			case COM_DT_VALUE:
			{
				SetValueOperation *operation = new SetValueOperation();
				operation->setValue(inputsocket->getEditorValueFloat());
				outputsocket->relinkConnections(operation->getOutputSocket(0));
				graph->addOperation(operation);
				break;
			}
			case COM_DT_COLOR:
			{
				SetColorOperation *operation = new SetColorOperation();
				float col[4];
				inputsocket->getEditorValueColor(col);
				operation->setChannels(col);
				outputsocket->relinkConnections(operation->getOutputSocket(0));
				graph->addOperation(operation);
				break;
			}
			case COM_DT_VECTOR:
			{
				SetVectorOperation *operation = new SetVectorOperation();
				float vec[3];
				inputsocket->getEditorValueVector(vec);
				operation->setVector(vec);
				outputsocket->relinkConnections(operation->getOutputSocket(0));
				graph->addOperation(operation);
				break;
			}
		}
	}
}
