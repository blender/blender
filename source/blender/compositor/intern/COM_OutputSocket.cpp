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

#include "COM_Socket.h"
#include "COM_Node.h"
#include "COM_SocketConnection.h"
#include "COM_NodeOperation.h"

OutputSocket::OutputSocket(DataType datatype) : Socket(datatype)
{
}

int OutputSocket::isOutputSocket() const { return true; }
const int OutputSocket::isConnected() const { return this->connections.size() != 0; }

void OutputSocket::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	NodeBase *node = this->getNode();
	if (node->isOperation()) {
		NodeOperation *operation = (NodeOperation *)node;
		if (operation->isResolutionSet()) {
			resolution[0] = operation->getWidth();
			resolution[1] = operation->getHeight();
		}
		else {
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}
}

void OutputSocket::addConnection(SocketConnection *connection)
{
	this->connections.push_back(connection);
}

void OutputSocket::relinkConnections(OutputSocket *relinkToSocket, bool single)
{
	if (isConnected()) {
		if (single) {
			SocketConnection *connection = this->connections[0];
			connection->setFromSocket(relinkToSocket);
			relinkToSocket->addConnection(connection);
			this->connections.erase(this->connections.begin());
		}
		else {
			unsigned int index;
			for (index = 0; index < this->connections.size(); index++) {
				SocketConnection *connection = this->connections[index];
				connection->setFromSocket(relinkToSocket);
				relinkToSocket->addConnection(connection);
			}
			this->connections.clear();
		}
	}
}
void OutputSocket::removeFirstConnection()
{
	SocketConnection *connection = this->connections[0];
	InputSocket *inputSocket = connection->getToSocket();
	if (inputSocket != NULL) {
		inputSocket->setConnection(NULL);
	}
	this->connections.erase(this->connections.begin());
}

void OutputSocket::clearConnections()
{
	while (this->isConnected()) {
		removeFirstConnection();
	}
}

WriteBufferOperation *OutputSocket::findAttachedWriteBufferOperation() const
{
	unsigned int index;
	for (index = 0; index < this->connections.size(); index++) {
		SocketConnection *connection = this->connections[index];
		NodeBase *node = connection->getToNode();
		if (node->isOperation()) {
			NodeOperation *operation = (NodeOperation *)node;
			if (operation->isWriteBufferOperation()) {
				return (WriteBufferOperation *)operation;
			}
		}
	}
	return NULL;
}

