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

#include "COM_NodeBase.h"
#include "string.h"
#include "COM_NodeOperation.h"
#include "BKE_node.h"
#include "COM_SetValueOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SocketConnection.h"
#include "COM_ExecutionSystem.h"

NodeBase::NodeBase()
{
	/* pass */
}


NodeBase::~NodeBase()
{
	while (!this->outputsockets.empty()) {
		delete (this->outputsockets.back());
		this->outputsockets.pop_back();
	}
	while (!this->inputsockets.empty()) {
		delete (this->inputsockets.back());
		this->inputsockets.pop_back();
	}
}

void NodeBase::addInputSocket(DataType datatype)
{
	this->addInputSocket(datatype, COM_SC_CENTER, NULL);
}

void NodeBase::addInputSocket(DataType datatype, InputSocketResizeMode resizeMode)
{
	this->addInputSocket(datatype, resizeMode, NULL);
}
void NodeBase::addInputSocket(DataType datatype, InputSocketResizeMode resizeMode, bNodeSocket *bSocket)
{
	InputSocket *socket = new InputSocket(datatype, resizeMode);
	socket->setEditorSocket(bSocket);
	socket->setNode(this);
	this->inputsockets.push_back(socket);
}

void NodeBase::addOutputSocket(DataType datatype)
{
	this->addOutputSocket(datatype, NULL);
	
}
void NodeBase::addOutputSocket(DataType datatype, bNodeSocket *bSocket)
{
	OutputSocket *socket = new OutputSocket(datatype);
	socket->setEditorSocket(bSocket);
	socket->setNode(this);
	this->outputsockets.push_back(socket);
}
const bool NodeBase::isInputNode() const
{
	return this->inputsockets.size() == 0;
}

OutputSocket *NodeBase::getOutputSocket(unsigned int index)
{
	BLI_assert(index < this->outputsockets.size());
	return this->outputsockets[index];
}

InputSocket *NodeBase::getInputSocket(unsigned int index)
{
	BLI_assert(index < this->inputsockets.size());
	return this->inputsockets[index];
}
