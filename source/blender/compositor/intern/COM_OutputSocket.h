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

#ifndef _COM_OutputSocket_h
#define _COM_OutputSocket_h

#include <vector>
#include "COM_Socket.h"
#include "COM_ChannelInfo.h"

using namespace std;
class SocketConnection;
class Node;
class InputSocket;
class WriteBufferOperation;

//#define COM_ST_INPUT 0
//#define COM_ST_OUTPUT 1

/**
 * @brief OutputSocket are sockets that can send data/input
 * @ingroup Model
 */
class OutputSocket : public Socket {
private:
	vector<SocketConnection *> connections;
		
	void removeFirstConnection();
public:
	OutputSocket(DataType datatype);
	OutputSocket(DataType datatype, int inputSocketDataTypeDeterminatorIndex);
	OutputSocket(OutputSocket *from);
	void addConnection(SocketConnection *connection);
	SocketConnection *getConnection(unsigned int index) { return this->connections[index]; }
	const int isConnected() const;
	int isOutputSocket() const;
	
	/**
	 * @brief determine the resolution of this socket
	 * @param resolution the result of this operation
	 * @param preferredResolution the preferrable resolution as no resolution could be determined
	 */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	
	/**
	 * @brief determine the actual data type and channel info.
	 */
	void relinkConnections(OutputSocket *relinkToSocket) { this->relinkConnections(relinkToSocket, false); };
	void relinkConnections(OutputSocket *relinkToSocket, bool single);
	const int getNumberOfConnections() { return connections.size(); }
	
	void clearConnections();
	
	/**
	 * @brief find a connected write buffer operation to this OutputSocket
	 * @return WriteBufferOperation or NULL
	 */
	WriteBufferOperation *findAttachedWriteBufferOperation() const;
	ChannelInfo *getChannelInfo(const int channelnumber);
	
private:

};
#endif
