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
 * LoopbackNetworkDeviceInterface derived from NG_NetworkDeviceInterface
 */

#include "NG_LoopBackNetworkDeviceInterface.h"
#include "NG_NetworkMessage.h"

// temporary debugging printf's
#ifdef NAN_NET_DEBUG
  #include <stdio.h>
#endif

NG_LoopBackNetworkDeviceInterface::NG_LoopBackNetworkDeviceInterface()
{
	m_currentQueue=0;
	Online();   // LoopBackdevices are 'online' immediately
}

NG_LoopBackNetworkDeviceInterface::~NG_LoopBackNetworkDeviceInterface()
{
}

// perhaps this should go to the shared/common implementation too
void NG_LoopBackNetworkDeviceInterface::NextFrame()
{
	// Release reference to the messages while emptying the queue
	while (m_messages[m_currentQueue].size() > 0) {
#ifdef NAN_NET_DEBUG
	printf("NG_LBNDI::NextFrame %d '%s'\n", m_currentQueue, m_messages[m_currentQueue][0]->GetSubject().ReadPtr());
#endif
		// Should do assert(m_events[0]);
		m_messages[m_currentQueue][0]->Release();
		m_messages[m_currentQueue].pop_front();
	}
	//m_messages[m_currentQueue].clear();

	m_currentQueue=1-m_currentQueue;
}

STR_String NG_LoopBackNetworkDeviceInterface::GetNetworkVersion()
{
	return LOOPBACK_NETWORK_VERSION;
}

void NG_LoopBackNetworkDeviceInterface::SendNetworkMessage(NG_NetworkMessage* nwmsg)
{
#ifdef NAN_NET_DEBUG
	printf("NG_LBNDI::SendNetworkMessage %d, '%s'->'%s' '%s' '%s'\n",
		   1-m_currentQueue,
		   nwmsg->GetDestinationName().ReadPtr(),
		   nwmsg->GetSenderName().ReadPtr(),
		   nwmsg->GetSubject().ReadPtr(),
		   nwmsg->GetMessageText().ReadPtr());
#endif
	int backqueue = 1-m_currentQueue;

	nwmsg->AddRef();
	m_messages[backqueue].push_back(nwmsg);
}

vector<NG_NetworkMessage*> NG_LoopBackNetworkDeviceInterface::RetrieveNetworkMessages()
{
	vector<NG_NetworkMessage*> messages;
	
	std::deque<NG_NetworkMessage*>::iterator mesit=m_messages[m_currentQueue].begin();
	for (; !(mesit == m_messages[m_currentQueue].end()); ++mesit)
	{

		// We don't increase the reference count for these messages. We
		// are passing a vector of messages in the interface and not
		// explicitily storing the messgaes for long term usage

		messages.push_back(*mesit);

#ifdef NAN_NET_DEBUG
		printf("NG_LBNDI::RetrieveNetworkMessages %d '%s'->'%s' '%s' '%s'\n",
			m_currentQueue,
			(*mesit)->GetDestinationName().ReadPtr(),
			(*mesit)->GetSenderName().ReadPtr(),
			(*mesit)->GetSubject().ReadPtr(),
			(*mesit)->GetMessageText().ReadPtr());
#endif
	}
	return messages;
}

