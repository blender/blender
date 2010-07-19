/**
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
 * NetworkSceneManagement generic implementation
 */
#include <stdio.h>
#include <MT_assert.h>
#include <algorithm>

#include "NG_NetworkScene.h"
#include "NG_NetworkDeviceInterface.h"
#include "NG_NetworkMessage.h"
#include "NG_NetworkObject.h"

NG_NetworkScene::NG_NetworkScene(NG_NetworkDeviceInterface* nic)
{
	m_networkdevice = nic;
}

NG_NetworkScene::~NG_NetworkScene()
{
	ClearAllMessageMaps();
}

/**
 * progress one frame, handle all network traffic
 */
void NG_NetworkScene::proceed(double curtime)
{
	if (!m_networkdevice) return;
	if (!m_networkdevice->IsOnline()) return;

	ClearAllMessageMaps();

	// read all NetworkMessages from the device
	vector<NG_NetworkMessage*> messages =
		m_networkdevice->RetrieveNetworkMessages();

	vector<NG_NetworkMessage*>::iterator mesit = messages.begin();
	for (; !(mesit == messages.end()); mesit++) {
		NG_NetworkMessage* message = (*mesit);
		vector<NG_NetworkMessage*>* tmplist=NULL;

		vector<NG_NetworkMessage*>** tmplistptr =
			m_messagesByDestinationName[message->GetDestinationName()];
		// if there is already a vector of messages, append, else create
		// a new vector and insert into map
		if (!tmplistptr) {
			tmplist = new vector<NG_NetworkMessage*>;
			m_messagesByDestinationName.insert(message->GetDestinationName(),
											   tmplist);
		} else {
			tmplist = *tmplistptr;
		}
		message->AddRef();
		tmplist->push_back(message);
		tmplist = NULL;

		tmplistptr = m_messagesBySenderName[message->GetSenderName()];
		// if there is already a vector of messages, append, else create
		// a new vector and insert into map
		if (!tmplistptr) {
			tmplist = new vector<NG_NetworkMessage*>;
			m_messagesBySenderName.insert(message->GetSenderName(), tmplist);
		}  else {
			tmplist = *tmplistptr;
		}
		message->AddRef();
		tmplist->push_back(message);
		tmplist = NULL;
		
		tmplistptr = m_messagesBySubject[message->GetSubject()];
		// if there is already a vector of messages, append, else create
		// a new vector and insert into map
		if (!tmplistptr) {
			tmplist = new vector<NG_NetworkMessage*>;
			m_messagesBySubject.insert(message->GetSubject(), tmplist);
		}  else {
			tmplist = *tmplistptr;
		}
		message->AddRef();
		tmplist->push_back(message);
		tmplist = NULL;
	}
}

/**
 * add a network object to the network scene
 */
void NG_NetworkScene::AddObject(NG_NetworkObject* object)
{
	if (! m_networkdevice->IsOnline()) return;

	const STR_String& name = object->GetName();
	m_networkObjects.insert(name, object);
}

/**
 * remove a network object from the network scene
 */
void NG_NetworkScene::RemoveObject(NG_NetworkObject* object)
{
	if (! m_networkdevice->IsOnline()) return;

	const STR_String& name = object->GetName();
	m_networkObjects.remove(name);
}

/**
 * remove all network scene objects at once
 */
void NG_NetworkScene::RemoveAllObjects()
{
	m_networkObjects.clear();
}

/**
 * get a single network object given its name
 */
NG_NetworkObject* NG_NetworkScene::FindNetworkObject(const STR_String& objname) {
	NG_NetworkObject *nwobj = NULL;
	if (! m_networkdevice->IsOnline()) return nwobj;

	NG_NetworkObject **nwobjptr = m_networkObjects[objname];
	if (nwobjptr) {
		nwobj = *nwobjptr;
	}

	return nwobj;
}

bool NG_NetworkScene::ConstraintsAreValid(
	const STR_String& from,
	const STR_String& subject,
	NG_NetworkMessage* message)
{
	vector<NG_NetworkMessage*>** fromlistptr =  m_messagesBySenderName[from];
	vector<NG_NetworkMessage*>** subjectlistptr =  m_messagesBySubject[subject];

	vector<NG_NetworkMessage*>* fromlist = (fromlistptr ? *fromlistptr : NULL);
	vector<NG_NetworkMessage*>* subjectlist = (subjectlistptr ? *subjectlistptr : NULL);
	
	return (
		( from.IsEmpty()    || (!fromlist ? false    : (!(std::find(fromlist->begin(), fromlist->end(), message)       == fromlist->end())))
		) &&
		( subject.IsEmpty() || (!subjectlist ? false : (!(std::find(subjectlist->begin(), subjectlist->end(), message) == subjectlist->end())))
		));
}

vector<NG_NetworkMessage*> NG_NetworkScene::FindMessages(
	const STR_String& to,
	const STR_String& from,
	const STR_String& subject,
	bool spamallowed)
{
	vector<NG_NetworkMessage*> foundmessages;
	bool notfound = false;

	// broad phase
	notfound = ((to.IsEmpty() || spamallowed) ? notfound : m_messagesByDestinationName[to] == NULL);
	if (!notfound)
		notfound = (from.IsEmpty() ? notfound : m_messagesBySenderName[from] == NULL);
	if (!notfound)
		notfound = (subject.IsEmpty() ? notfound : m_messagesBySubject[subject] == NULL);
	if (notfound) {
		// it's definately NOT in the scene, so stop looking
	} else { // narrow phase
		// possibly it's there, but maybe not (false hit)
		if (to.IsEmpty()) {
			// take all messages, and check other fields
			MT_assert(!"objectnames that are empty are not valid, so make it a hobby project :)\n");
		} else {
			//todo: find intersection of messages (that are in other 2 maps)
			vector<NG_NetworkMessage*>** tolistptr = m_messagesByDestinationName[to];
			if (tolistptr) {
				vector<NG_NetworkMessage*>* tolist = *tolistptr;
				vector<NG_NetworkMessage*>::iterator listit;
				for (listit=tolist->begin();!(listit==tolist->end());listit++) {
					NG_NetworkMessage* message = *listit;
					if (ConstraintsAreValid(from, subject, message)) {
						message->AddRef();
						foundmessages.push_back(message);
					}
				} 
			}
			// TODO find intersection of messages (that are in other 2 maps)
			if (spamallowed) {
				tolistptr = m_messagesByDestinationName[""];
				if (tolistptr) {
					vector<NG_NetworkMessage*>* tolist = *tolistptr;
					vector<NG_NetworkMessage*>::iterator listit;
					for (listit=tolist->begin();!(listit==tolist->end());listit++) {
						NG_NetworkMessage* message = *listit;
						if (ConstraintsAreValid(from, subject, message)) {
							message->AddRef();
							foundmessages.push_back(message);
						}
					} 
				}
			}
		}
	} 
	return foundmessages;
}

void NG_NetworkScene::SendMessage(
	const STR_String& to,
	const STR_String& from,
	const STR_String& subject,
	const STR_String& message)
{
	NG_NetworkMessage* msg = new NG_NetworkMessage(to, from, subject, message);
	m_networkdevice->SendNetworkMessage(msg);
	msg->Release();
}

void NG_NetworkScene::ClearAllMessageMaps(void)
{
	ClearMessageMap(m_messagesByDestinationName);
	ClearMessageMap(m_messagesBySenderName);
	ClearMessageMap(m_messagesBySubject);
}

void NG_NetworkScene::ClearMessageMap(TMessageMap& map)
{
	// Release the messages in the map
	for (int i = 0; i < map.size(); i++) {
		vector<NG_NetworkMessage*>* msglist;
		msglist = *(map.at(i));

		// Iterate through the current vector and release all it's messages
		vector<NG_NetworkMessage*>::iterator msgit;
		for (msgit = msglist->begin(); msgit != msglist->end(); msgit++) {
			(*msgit)->Release();
		}

		// Delete the actual vector
		delete (msglist);
	}

	// Empty the map
	map.clear();
}

