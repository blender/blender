/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file NG_NetworkDeviceInterface.h
 *  \ingroup bgenet
 *  \brief Functions like (de)initialize network, get library version
 * To be derived by loopback and network libraries
 */

#ifndef __NG_NETWORKDEVICEINTERFACE_H__
#define __NG_NETWORKDEVICEINTERFACE_H__

#include "NG_NetworkMessage.h"
#include <vector>

class NG_NetworkDeviceInterface
{
private:
	// candidates for shared/common implementation class
	bool m_online;
public:
	NG_NetworkDeviceInterface() {}
	virtual ~NG_NetworkDeviceInterface() {}

	virtual void NextFrame()=0;

	/**
	 * Mark network connection online
	 */
	void Online(void) { m_online = true; }
	/**
	 * Mark network connection offline
	 */
	void Offline(void) { m_online = false; }
	/**
	 * Is the network connection established ?
	 */
	bool IsOnline(void) { return m_online; }

	virtual bool Connect(char *address, unsigned int port, char *password,
	                     unsigned int localport, unsigned int timeout)=0;
	virtual bool Disconnect(void)=0;

	virtual void SendNetworkMessage(NG_NetworkMessage* msg)=0;
	/**
	 * read NG_NetworkMessage from library buffer, may be
	 * irrelevant for loopbackdevices
	 */
	
	virtual std::vector<NG_NetworkMessage*> RetrieveNetworkMessages()=0;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:NG_NetworkDeviceInterface")
#endif
};

#endif  /* __NG_NETWORKDEVICEINTERFACE_H__ */
