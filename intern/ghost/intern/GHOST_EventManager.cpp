/**
 * $Id$
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

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	May 14, 2001
 */

#include "GHOST_EventManager.h"
#include <algorithm>
#include "GHOST_Debug.h"


GHOST_EventManager::GHOST_EventManager()
{
}


GHOST_EventManager::~GHOST_EventManager()
{
	disposeEvents();

	TConsumerVector::iterator iter= m_consumers.begin();
	while (iter != m_consumers.end())
	{
		GHOST_IEventConsumer* consumer = *iter;
		delete consumer;
		m_consumers.erase(iter);
		iter = m_consumers.begin();
	}
}


GHOST_TUns32 GHOST_EventManager::getNumEvents()
{
	return (GHOST_TUns32) m_events.size();
}


GHOST_TUns32 GHOST_EventManager::getNumEvents(GHOST_TEventType type)
{
	GHOST_TUns32 numEvents = 0;
	TEventStack::iterator p;
	for (p = m_events.begin(); p != m_events.end(); p++) {
		if ((*p)->getType() == type) {
			numEvents++;
		}
	}
	return numEvents;
}


GHOST_IEvent* GHOST_EventManager::peekEvent()
{
	GHOST_IEvent* event = 0;
	if (m_events.size() > 0) {
		event = m_events.back();
	}
	return event;
}


GHOST_TSuccess GHOST_EventManager::pushEvent(GHOST_IEvent* event)
{
	GHOST_TSuccess success;
	GHOST_ASSERT(event, "invalid event");
	if (m_events.size() < m_events.max_size()) {
		m_events.push_front(event);
		success = GHOST_kSuccess;
	}
	else {
		success = GHOST_kFailure;
	}
	return success;
}


bool GHOST_EventManager::dispatchEvent(GHOST_IEvent* event)
{
	bool handled;
	if (event) {
		handled = true;
		TConsumerVector::iterator iter;
		for (iter = m_consumers.begin(); iter != m_consumers.end(); iter++) {
			if ((*iter)->processEvent(event)) {
				handled = false;
			}
		}
	}
	else {
		handled = false;
	}
	return handled;
}


bool GHOST_EventManager::dispatchEvent()
{
	GHOST_IEvent* event = popEvent(); 
	bool handled = false;
	if (event) {
		handled = dispatchEvent(event);
		delete event;
	}
	return handled;
}


bool GHOST_EventManager::dispatchEvents()
{
	bool handled;
	if (getNumEvents()) {
		handled = true;
		while (getNumEvents()) {
			if (!dispatchEvent()) {
				handled = false;
			}
		}
	}
	else {
		handled = false;
	}
	return handled;
}


GHOST_TSuccess GHOST_EventManager::addConsumer(GHOST_IEventConsumer* consumer)
{
	GHOST_TSuccess success;
	GHOST_ASSERT(consumer, "invalid consumer");
	
	// Check to see whether the consumer is already in our list
	TConsumerVector::const_iterator iter = std::find(m_consumers.begin(), m_consumers.end(), consumer);

	if (iter == m_consumers.end()) {
		// Add the consumer
		m_consumers.push_back(consumer);
		success = GHOST_kSuccess;
	}
	else {
		success = GHOST_kFailure;
	}
	return success;
}


GHOST_TSuccess GHOST_EventManager::removeConsumer(GHOST_IEventConsumer* consumer)
{
	GHOST_TSuccess success;
	GHOST_ASSERT(consumer, "invalid consumer");

	// Check to see whether the consumer is in our list
	TConsumerVector::iterator iter = std::find(m_consumers.begin(), m_consumers.end(), consumer);

	if (iter != m_consumers.end()) {
		// Remove the consumer
		m_consumers.erase(iter);
		success = GHOST_kSuccess;
	}
	else {
		success = GHOST_kFailure;
	}
	return success;
}


void GHOST_EventManager::removeWindowEvents(GHOST_IWindow* window)
{
	TEventStack::iterator iter;
	iter = m_events.begin();
	while (iter != m_events.end())
	{
		GHOST_IEvent* event = *iter;
		if (event->getWindow() == window)
		{
            GHOST_PRINT("GHOST_EventManager::removeWindowEvents(): removing event\n");
			/*
			 * Found an event for this window, remove it.
			 * The iterator will become invalid.
			 */
			delete event;
			m_events.erase(iter);
			iter = m_events.begin();
		}
		else
		{
			iter++;
		}
	}
}

void GHOST_EventManager::removeTypeEvents(GHOST_TEventType type, GHOST_IWindow* window)
{
	TEventStack::iterator iter;
	iter = m_events.begin();
	while (iter != m_events.end())
	{
		GHOST_IEvent* event = *iter;
		if ((event->getType() == type) && (!window || (event->getWindow() == window)))
		{
            GHOST_PRINT("GHOST_EventManager::removeTypeEvents(): removing event\n");
			/*
			 * Found an event of this type for the window, remove it.
			 * The iterator will become invalid.
			 */
			delete event;
			m_events.erase(iter);
			iter = m_events.begin();
		}
		else
		{
			iter++;
		}
	}
}


GHOST_IEvent* GHOST_EventManager::popEvent()
{
	GHOST_IEvent* event = peekEvent();
	if (event) {
		m_events.pop_back();
	}
	return event;
}


void GHOST_EventManager::disposeEvents()
{
	while (m_events.size() > 0) {
		GHOST_ASSERT(m_events[0], "invalid event");
		delete m_events[0];
		m_events.pop_front();
	}
}
