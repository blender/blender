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

#include <typeinfo>
#include <stdio.h>

#include "COM_NodeOperation.h"
#include "COM_InputSocket.h"
#include "COM_SocketConnection.h"
#include "COM_defines.h"

NodeOperation::NodeOperation() : NodeBase()
{
	this->m_resolutionInputSocketIndex = 0;
	this->m_complex = false;
	this->m_width = 0;
	this->m_height = 0;
	this->m_openCL = false;
	this->m_btree = NULL;
}

void NodeOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	unsigned int temp[2];
	unsigned int temp2[2];
	vector<InputSocket *> &inputsockets = this->getInputSockets();
	
	for (unsigned int index = 0; index < inputsockets.size(); index++) {
		InputSocket *inputSocket = inputsockets[index];
		if (inputSocket->isConnected()) {
			if (index == this->m_resolutionInputSocketIndex) {
				inputSocket->determineResolution(resolution, preferredResolution);
				temp2[0] = resolution[0];
				temp2[1] = resolution[1];
				break;
			}
		}
	}
	for (unsigned int index = 0; index < inputsockets.size(); index++) {
		InputSocket *inputSocket = inputsockets[index];
		if (inputSocket->isConnected()) {
			if (index != this->m_resolutionInputSocketIndex) {
				inputSocket->determineResolution(temp, temp2);
			}
		}
	}
}
void NodeOperation::setResolutionInputSocketIndex(unsigned int index)
{
	this->m_resolutionInputSocketIndex = index;
}
void NodeOperation::initExecution()
{
	/* pass */
}

void NodeOperation::initMutex()
{
	BLI_mutex_init(&this->m_mutex);
}

void NodeOperation::lockMutex()
{
	BLI_mutex_lock(&this->m_mutex);
}

void NodeOperation::unlockMutex()
{
	BLI_mutex_unlock(&this->m_mutex);
}

void NodeOperation::deinitMutex()
{
	BLI_mutex_end(&this->m_mutex);
}

void NodeOperation::deinitExecution()
{
	/* pass */
}
SocketReader *NodeOperation::getInputSocketReader(unsigned int inputSocketIndex)
{
	return this->getInputSocket(inputSocketIndex)->getReader();
}
NodeOperation *NodeOperation::getInputOperation(unsigned int inputSocketIndex)
{
	return this->getInputSocket(inputSocketIndex)->getOperation();
}

void NodeOperation::getConnectedInputSockets(vector<InputSocket *> *sockets)
{
	vector<InputSocket *> &inputsockets = this->getInputSockets();
	for (vector<InputSocket *>::iterator iterator = inputsockets.begin(); iterator != inputsockets.end(); iterator++) {
		InputSocket *socket = *iterator;
		if (socket->isConnected()) {
			sockets->push_back(socket);
		}
	}
}

bool NodeOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (this->isInputNode()) {
		BLI_rcti_init(output, input->xmin, input->xmax, input->ymin, input->ymax);
		return false;
	}
	else {
		unsigned int index;
		vector<InputSocket *> &inputsockets = this->getInputSockets();
	
		for (index = 0; index < inputsockets.size(); index++) {
			InputSocket *inputsocket = inputsockets[index];
			if (inputsocket->isConnected()) {
				NodeOperation *inputoperation = (NodeOperation *)inputsocket->getConnection()->getFromNode();
				bool result = inputoperation->determineDependingAreaOfInterest(input, readOperation, output);
				if (result) {
					return true;
				}
			}
		}
		return false;
	}
}
