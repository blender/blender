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
#include "COM_SocketProxyOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"

SocketProxyNode::SocketProxyNode(bNode *editorNode, bNodeSocket *editorInput, bNodeSocket *editorOutput) : Node(editorNode, false)
{
	DataType dt;

	dt = COM_DT_VALUE;
	if (editorInput->type == SOCK_RGBA) dt = COM_DT_COLOR;
	if (editorInput->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
	this->addInputSocket(dt, editorInput);

	dt = COM_DT_VALUE;
	if (editorOutput->type == SOCK_RGBA) dt = COM_DT_COLOR;
	if (editorOutput->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
	this->addOutputSocket(dt, editorOutput);
}

void SocketProxyNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	NodeOperationOutput *proxy_output = converter.addInputProxy(getInputSocket(0));
	converter.mapOutputSocket(getOutputSocket(), proxy_output);
}


SocketBufferNode::SocketBufferNode(bNode *editorNode, bNodeSocket *editorInput, bNodeSocket *editorOutput) : Node(editorNode, false)
{
	DataType dt;

	dt = COM_DT_VALUE;
	if (editorInput->type == SOCK_RGBA) dt = COM_DT_COLOR;
	if (editorInput->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
	this->addInputSocket(dt, editorInput);

	dt = COM_DT_VALUE;
	if (editorOutput->type == SOCK_RGBA) dt = COM_DT_COLOR;
	if (editorOutput->type == SOCK_VECTOR) dt = COM_DT_VECTOR;
	this->addOutputSocket(dt, editorOutput);
}

void SocketBufferNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	NodeOutput *output = this->getOutputSocket(0);
	NodeInput *input = this->getInputSocket(0);
	
	WriteBufferOperation *writeOperation = new WriteBufferOperation();
	ReadBufferOperation *readOperation = new ReadBufferOperation();
	readOperation->setMemoryProxy(writeOperation->getMemoryProxy());
	converter.addOperation(writeOperation);
	converter.addOperation(readOperation);
	
	converter.mapInputSocket(input, writeOperation->getInputSocket(0));
	converter.mapOutputSocket(output, readOperation->getOutputSocket());
}
