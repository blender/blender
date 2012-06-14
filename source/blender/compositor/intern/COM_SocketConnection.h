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

#ifndef _COM_SocketConnection_h
#define _COM_SocketConnection_h

#include "DNA_node_types.h"
#include "COM_Node.h"
#include "COM_Socket.h"
#include "COM_ChannelInfo.h"

/**
 * @brief An SocketConnection is an connection between an InputSocket and an OutputSocket.
 *
 * <pre>
 * +----------+     To InputSocket +----------+
 * | From     |  SocketConnection \| To Node  |
 * | Node     *====================*          |
 * |          |\                   |          |
 * |          | From OutputSocket  +----------+
 * +----------+
 * </pre>
 * @ingroup Model
 * @see InputSocket
 * @see OutputSocket
 */
class SocketConnection {
private:
	/**
	 * @brief Startpoint of the connection
	 */
	OutputSocket *fromSocket;
	
	/**
	 * @brief Endpoint of the connection
	 */
	InputSocket *toSocket;
	
	/**
	 * @brief has the resize already been done for this connection
	 */
	bool ignoreResizeCheck;
public:
	SocketConnection();
	
	/**
	 * @brief set the startpoint of the connection
	 * @param fromsocket
	 */
	void setFromSocket(OutputSocket *fromsocket);
	
	/**
	 * @brief get the startpoint of the connection
	 * @return from OutputSocket
	 */
	OutputSocket *getFromSocket() const;
	
	/**
	 * @brief set the endpoint of the connection
	 * @param tosocket
	 */
	void setToSocket(InputSocket *tosocket);
	
	/**
	 * @brief get the endpoint of the connection
	 * @return to InputSocket
	 */
	InputSocket *getToSocket() const;
	
	/**
	 * @brief check if this connection is valid
	 */
	bool isValid() const;
	
	/**
	 * @brief return the Node where this connection is connected from
	 */
	NodeBase *getFromNode() const;
	
	/**
	 * @brief return the Node where this connection is connected to
	 */
	NodeBase *getToNode() const;
	
	/**
	 * @brief set, whether the resize has already been done for this SocketConnection
	 */
	void setIgnoreResizeCheck(bool check) { this->ignoreResizeCheck = check; }
	
	/**
	 * @brief has the resize already been done for this SocketConnection
	 */
	bool isIgnoreResizeCheck() const { return this->ignoreResizeCheck; }
	
	/**
	 * @brief does this SocketConnection need resolution conversion
	 * @note PreviewOperation's will be ignored
	 * @note Already converted SocketConnection's will be ignored
	 * @return needs conversion [true:false]
	 */
	bool needsResolutionConversion() const;
};

#endif
