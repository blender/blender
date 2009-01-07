/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * LoopbackNetworkDeviceInterface derived from NG_NetworkDeviceInterface
 */
#ifndef NG_LOOPBACKNETWORKDEVICEINTERFACE_H
#define NG_LOOPBACKNETWORKDEVICEINTERFACE_H

#include <deque>
#include "NG_NetworkDeviceInterface.h"

class NG_LoopBackNetworkDeviceInterface : public NG_NetworkDeviceInterface
{
	enum {
		LOOPBACK_NETWORK_VERSION=28022001
	};
	
	std::deque<NG_NetworkMessage*> m_messages[2];
	int		m_currentQueue;

public:
	NG_LoopBackNetworkDeviceInterface();
	virtual ~NG_LoopBackNetworkDeviceInterface();

	/**
	  * Clear message buffer
	  */
	virtual void NextFrame();

	bool Connect(char *address, unsigned int port, char *password,
		     unsigned int localport, unsigned int timeout) {
	    return true;}
	bool Disconnect(void) {return true;}

	virtual void SendNetworkMessage(class NG_NetworkMessage* msg);
	virtual vector<NG_NetworkMessage*>		RetrieveNetworkMessages();

	STR_String GetNetworkVersion();
};

#endif //NG_LOOPBACKNETWORKDEVICEINTERFACE_H

