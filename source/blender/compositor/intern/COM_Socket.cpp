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

Socket::Socket(DataType datatype)
{
	this->m_datatype = datatype;
	this->m_editorSocket = NULL;
	this->m_node = NULL;
}

DataType Socket::getDataType() const
{
	return this->m_datatype;
}

int Socket::isInputSocket() const { return false; }
int Socket::isOutputSocket() const { return false; }
const int Socket::isConnected() const { return false; }
void Socket::setNode(NodeBase *node) { this->m_node = node; }
NodeBase *Socket::getNode() const { return this->m_node; }
