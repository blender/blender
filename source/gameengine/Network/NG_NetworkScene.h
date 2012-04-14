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

/** \file NG_NetworkScene.h
 *  \ingroup bgenet
 *  \brief NetworkSceneManagement generic class
 */
#ifndef __NG_NETWORKSCENE_H__
#define __NG_NETWORKSCENE_H__

#include "CTR_Map.h"
#include "STR_HashedString.h"
#include <vector>

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

//MSVC defines SendMessage as a win api function, even though we aren't using it
#ifdef SendMessage
	#undef SendMessage
#endif

using namespace std;

class NG_NetworkDeviceInterface;

class NG_NetworkScene
{
	class NG_NetworkDeviceInterface *m_networkdevice;	
	CTR_Map<STR_HashedString, class NG_NetworkObject *> m_networkObjects;

	// CTR_Maps used as a 'Bloom' filter
	typedef CTR_Map<STR_HashedString, std::vector<class NG_NetworkMessage*>* > TMessageMap;
	TMessageMap m_messagesByDestinationName;
	TMessageMap m_messagesBySenderName;
	TMessageMap m_messagesBySubject;

public:
	NG_NetworkScene(NG_NetworkDeviceInterface *nic);
	~NG_NetworkScene();

	/**
	 * progress one frame, handle all network traffic
	 */
	void proceed(double curtime);

	/**
	 * add a networkobject to the scene
	 */
	void AddObject(NG_NetworkObject* object);

	/**
	 * remove a networkobject to the scene
	 */
	void RemoveObject(NG_NetworkObject* object);

	/**
	 * remove all objects at once
	 */
	void RemoveAllObjects();

	/**
	 *	send a message (ascii text) over the network
	 */
	void SendMessage(const STR_String& to,const STR_String& from,const STR_String& subject,const STR_String& message);

	/**
	 * find an object by name
	 */
	NG_NetworkObject* FindNetworkObject(const STR_String& objname);

	bool	ConstraintsAreValid(const STR_String& from,const STR_String& subject,class NG_NetworkMessage* message);
	vector<NG_NetworkMessage*> FindMessages(const STR_String& to,const STR_String& from,const STR_String& subject,bool spamallowed);

protected:
	/**
	 * Releases messages in message map members.
	 */
	void ClearAllMessageMaps(void);

	/**
	 * Releases messages for the given message map.
	 * \param map	Message map with messages.
	 */
	void ClearMessageMap(TMessageMap& map);	


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:NG_NetworkScene"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__NG_NETWORKSCENE_H__

