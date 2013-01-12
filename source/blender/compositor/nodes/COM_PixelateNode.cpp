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

#include "COM_PixelateNode.h"

#include "COM_PixelateOperation.h"
#include "COM_ExecutionSystem.h"

PixelateNode::PixelateNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void PixelateNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *inputSocket = this->getInputSocket(0);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	DataType datatype = inputSocket->getDataType();
	if (inputSocket->isConnected()) {
		SocketConnection *connection = inputSocket->getConnection();
		OutputSocket *otherOutputSocket = connection->getFromSocket();
		datatype = otherOutputSocket->getDataType();
	}

	PixelateOperation *operation = new PixelateOperation(datatype);
	inputSocket->relinkConnections(operation->getInputSocket(0), 0, graph);
	outputSocket->relinkConnections(operation->getOutputSocket(0));
	graph->addOperation(operation);
}
