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
#include "COM_ExecutionSystem.h"

InputSocket::InputSocket(DataType datatype) : Socket(datatype)
{
	this->m_connection = NULL;
	this->m_resizeMode = COM_SC_CENTER;
}
InputSocket::InputSocket(DataType datatype, InputSocketResizeMode resizeMode) : Socket(datatype)
{
	this->m_connection = NULL;
	this->m_resizeMode = resizeMode;
}

InputSocket::InputSocket(InputSocket *from) : Socket(from->getDataType())
{
	this->m_connection = NULL;
	this->m_resizeMode = from->getResizeMode();
}

int InputSocket::isInputSocket() const { return true; }
const int InputSocket::isConnected() const { return this->m_connection != NULL; }

void InputSocket::setConnection(SocketConnection *connection)
{
	this->m_connection = connection;
}
SocketConnection *InputSocket::getConnection()
{
	return this->m_connection;
}

void InputSocket::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	if (this->isConnected()) {
		this->m_connection->getFromSocket()->determineResolution(resolution, preferredResolution);
	}
	else {
		return;
	}
}

void InputSocket::relinkConnections(InputSocket *relinkToSocket)
{
	if (!isConnected()) {
		return;
	}
	SocketConnection *connection = this->getConnection();
	connection->setToSocket(relinkToSocket);
	relinkToSocket->setConnection(connection);
	this->setConnection(NULL);
}

void InputSocket::relinkConnectionsDuplicate(InputSocket *relinkToSocket, int editorNodeInputSocketIndex, ExecutionSystem *graph)
{
	if (!this->isConnected()) {
		Node *node = (Node *)this->getNode();
		switch (this->getDataType()) {
			case COM_DT_COLOR:
				node->addSetColorOperation(graph, relinkToSocket, editorNodeInputSocketIndex);
				break;
			case COM_DT_VECTOR:
				node->addSetVectorOperation(graph, relinkToSocket, editorNodeInputSocketIndex);
				break;
			case COM_DT_VALUE:
				node->addSetValueOperation(graph, relinkToSocket, editorNodeInputSocketIndex);
				break;
		}
		return;
	}
	SocketConnection *newConnection = new SocketConnection();
	OutputSocket *fromSocket = this->getConnection()->getFromSocket();
	newConnection->setToSocket(relinkToSocket);
	newConnection->setFromSocket(fromSocket);
	relinkToSocket->setConnection(newConnection);
	fromSocket->addConnection(newConnection);
	graph->addSocketConnection(newConnection);
}

void InputSocket::relinkConnections(InputSocket *relinkToSocket,  int editorNodeInputSocketIndex, ExecutionSystem *graph)
{
	if (isConnected()) {
		relinkConnections(relinkToSocket);
	}
	else {
		Node *node = (Node *)this->getNode();
		switch (this->getDataType()) {
			case COM_DT_COLOR:
				node->addSetColorOperation(graph, relinkToSocket, editorNodeInputSocketIndex);
				break;
			case COM_DT_VECTOR:
				node->addSetVectorOperation(graph, relinkToSocket, editorNodeInputSocketIndex);
				break;
			case COM_DT_VALUE:
				node->addSetValueOperation(graph, relinkToSocket, editorNodeInputSocketIndex);
				break;
		}
	}
}

void InputSocket::unlinkConnections(ExecutionSystem *system)
{
	SocketConnection *connection = getConnection();
	if (connection) {
		system->removeSocketConnection(connection);
		connection->getFromSocket()->removeConnection(connection);
		setConnection(NULL);
		delete connection;
	}
}

bool InputSocket::isStatic()
{
	if (isConnected()) {
		NodeBase *node = this->getConnection()->getFromNode();
		if (node) {
			return node->isStatic();
		}
	}
	return true;
}
SocketReader *InputSocket::getReader()
{
	return this->getOperation();
}

NodeOperation *InputSocket::getOperation() const
{
	if (isConnected()) {
		return (NodeOperation *)this->m_connection->getFromSocket()->getNode();
	}
	else {
		return NULL;
	}
}
