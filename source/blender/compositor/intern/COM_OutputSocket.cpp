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

OutputSocket::OutputSocket(DataType datatype) :Socket(datatype)
{
	this->inputSocketDataTypeDeterminatorIndex = -1;
}
OutputSocket::OutputSocket(DataType datatype, int inputSocketDataTypeDeterminatorIndex) :Socket(datatype)
{
	this->inputSocketDataTypeDeterminatorIndex = inputSocketDataTypeDeterminatorIndex;
}

OutputSocket::OutputSocket(OutputSocket *from): Socket(from->getDataType())
{
	this->inputSocketDataTypeDeterminatorIndex = from->getInputSocketDataTypeDeterminatorIndex();	
}

int OutputSocket::isOutputSocket() const { return true; }
const int OutputSocket::isConnected() const { return this->connections.size()!=0; }

void OutputSocket::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	NodeBase *node = this->getNode();
	if (node->isOperation()) {
		NodeOperation *operation = (NodeOperation*)node;
		if (operation->isResolutionSet()) {
			resolution[0] = operation->getWidth();
			resolution[1] = operation->getHeight();
		} else {
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}
}

void OutputSocket::determineActualDataType()
{
	DataType actualDatatype = this->getNode()->determineActualDataType(this);

	/** @todo: set the channel info needs to be moved after integration with OCIO */
	this->channelinfo[0].setNumber(0);
	this->channelinfo[1].setNumber(1);
	this->channelinfo[2].setNumber(2);
	this->channelinfo[3].setNumber(3);
	switch (actualDatatype) {
	case COM_DT_VALUE:
		this->channelinfo[0].setType(COM_CT_Value);
		break;
	case COM_DT_VECTOR:
		this->channelinfo[0].setType(COM_CT_X);
		this->channelinfo[1].setType(COM_CT_Y);
		this->channelinfo[2].setType(COM_CT_Z);
		break;
	case COM_DT_COLOR:
		this->channelinfo[0].setType(COM_CT_ColorComponent);
		this->channelinfo[1].setType(COM_CT_ColorComponent);
		this->channelinfo[2].setType(COM_CT_ColorComponent);
		this->channelinfo[3].setType(COM_CT_Alpha);
		break;
	default:
		break;
	}

	this->setActualDataType(actualDatatype);
	this->fireActualDataType();
}

void OutputSocket::addConnection(SocketConnection *connection)
{
	this->connections.push_back(connection);
}

void OutputSocket::fireActualDataType()
{
	unsigned int index;
	for (index = 0 ; index < this->connections.size();index ++) {
		SocketConnection *connection = this->connections[index];
		connection->getToSocket()->notifyActualInputType(this->getActualDataType());
	}
}
void OutputSocket::relinkConnections(OutputSocket *relinkToSocket, bool single)
{
	if (isConnected()) {
		if (single) {
			SocketConnection *connection = this->connections[0];
			connection->setFromSocket(relinkToSocket);
			relinkToSocket->addConnection(connection);
//			relinkToSocket->setActualDataType(this->getActualDataType());
			this->connections.erase(this->connections.begin());
		}
		else {
			unsigned int index;
			for (index = 0 ; index < this->connections.size();index ++) {
				SocketConnection *connection = this->connections[index];
				connection->setFromSocket(relinkToSocket);
				relinkToSocket->addConnection(connection);
			}
//			relinkToSocket->setActualDataType(this->getActualDataType());
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
	for (index = 0 ; index < this->connections.size();index++) {
		SocketConnection *connection = this->connections[index];
		NodeBase *node = connection->getToNode();
		if (node->isOperation()) {
			NodeOperation *operation = (NodeOperation*)node;
			if (operation->isWriteBufferOperation()) {
				return (WriteBufferOperation*)operation;
			}
		}
	}
	return NULL;
}

ChannelInfo *OutputSocket::getChannelInfo(const int channelnumber)
{
	return &this->channelinfo[channelnumber];
}

