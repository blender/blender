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

#include "COM_SocketConnection.h"
#include "COM_NodeOperation.h"

SocketConnection::SocketConnection()
{
	this->fromSocket = NULL;
	this->toSocket = NULL;
	this->setIgnoreResizeCheck(false);
}

void SocketConnection::setFromSocket(OutputSocket *fromsocket)
{
	if (fromsocket == NULL) {
		throw "ERROR";
	}
	this->fromSocket = fromsocket;
}

OutputSocket *SocketConnection::getFromSocket() const {return this->fromSocket;}
void SocketConnection::setToSocket(InputSocket *tosocket)
{
	if (tosocket == NULL) {
		throw "ERROR";
	}
	this->toSocket = tosocket;
}

InputSocket *SocketConnection::getToSocket() const {return this->toSocket;}

NodeBase *SocketConnection::getFromNode() const
{
	if (this->getFromSocket() == NULL) {
		return NULL;
	}
	else {
		return this->getFromSocket()->getNode();
	}
}
NodeBase *SocketConnection::getToNode() const
{
	if (this->getToSocket() == NULL) {
		return NULL;
	}
	else {
		return this->getToSocket()->getNode();
	}
}
bool SocketConnection::isValid() const
{
	if ((this->getToSocket() != NULL && this->getFromSocket() != NULL)) {
		if (this->getFromNode()->isOperation() && this->getToNode()->isOperation()) {
			return true;
		}
	}
	return false;
}

bool SocketConnection::needsResolutionConversion() const
{
	if (this->ignoreResizeCheck) {return false;}
	NodeOperation *fromOperation = (NodeOperation*)this->getFromNode();
	NodeOperation *toOperation = (NodeOperation*)this->getToNode();
	if (this->toSocket->getResizeMode() == COM_SC_NO_RESIZE) {return false;}
	const unsigned int fromWidth = fromOperation->getWidth();
	const unsigned int fromHeight = fromOperation->getHeight();
	const unsigned int toWidth = toOperation->getWidth();
	const unsigned int toHeight = toOperation->getHeight();

	if (fromWidth == toWidth && fromHeight == toHeight) {
		return false;
	}
	return true;
}
