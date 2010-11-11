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

#include "GHOST_System.h"
#include "GHOST_IWindow.h"
#include "GHOST_WindowManager.h"
#include "GHOST_Event.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventWheel.h"
#include "GHOST_EventTrackpad.h"

GHOST_EventManager::GHOST_EventManager()
{
	m_playfile = m_recfile = NULL;
	m_lasttime = 0;
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

	
GHOST_TSuccess GHOST_EventManager::beginRecord(FILE *file)
{
	if (m_playfile || !file)
		return GHOST_kFailure;
		
	m_recfile = file;
	return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_EventManager::endRecord()
{
	if (!m_recfile)
		return GHOST_kFailure;
		
	m_recfile = NULL;
	return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_EventManager::playbackEvents(FILE *file)
{
	if (m_recfile || !file)
		return GHOST_kFailure;
		
	m_playfile = file;
	return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_EventManager::pushEvent(GHOST_IEvent* event)
{
	GHOST_TSuccess success;
	GHOST_ASSERT(event, "invalid event");
	if (m_events.size() < m_events.max_size()) {
		m_events.push_front(event);
		success = GHOST_kSuccess;
		if (m_recfile) {
			GHOST_System *sys;
			GHOST_ModifierKeys keys;
			GHOST_TInt32 x, y;
			char buf[256];

			sys = reinterpret_cast<GHOST_System*>(GHOST_ISystem::getSystem());
			
			/*write event parent class data*/
			event->writeheader(buf);
			fprintf(m_recfile, "%s\n", buf);
			
			/*write child class data*/
			event->serialize(buf);
			fprintf(m_recfile, "%s\n", buf);
			
			/*write modifier key states*/
			sys->getModifierKeys(keys);
			fprintf(m_recfile, "lshift: %d rshift: %d lalt: %d ralt: %d lctrl: %d rctrl: %d command: %d\n", 
				(int)keys.get(GHOST_kModifierKeyLeftShift),
				(int)keys.get(GHOST_kModifierKeyRightShift), 
				(int)keys.get(GHOST_kModifierKeyLeftAlt), 
				(int)keys.get(GHOST_kModifierKeyRightAlt), 
				(int)keys.get(GHOST_kModifierKeyLeftControl), 
				(int)keys.get(GHOST_kModifierKeyRightControl), 
				(int)keys.get(GHOST_kModifierKeyCommand));
			fflush(m_recfile);
			
			sys->getCursorPosition(x, y);

			/*write mouse cursor state*/
			fprintf(m_recfile, "mcursorstate: %d %d\n", x, y);
   		}
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

bool GHOST_EventManager::playingEvents(bool *hasevent) {
	if (hasevent && m_events.size()) {
		GHOST_IEvent *event = m_events[m_events.size()-1];
		GHOST_System *sys;

		sys = reinterpret_cast<GHOST_System*>(GHOST_ISystem::getSystem());
		*hasevent = event->getType()==0 || sys->getMilliSeconds()-m_lasttime > event->getTime();
		
		if (event->getType()==0)
			popEvent();
	} else if (hasevent) 
		*hasevent = true;
	
	return m_playfile != NULL;
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
	bool handled = false;
	
	if (m_recfile && getNumEvents()) {
		fprintf(m_recfile, "break\n");
	}
	
	if (m_playfile) {
		GHOST_IEvent *event = NULL;
		GHOST_System *sys;
		GHOST_WindowManager *wm;
		GHOST_TInt32 x, y;
		GHOST_ModifierKeys modkeys;
		std::vector<GHOST_IWindow *> windows;
		double lasttime = -1.0;
		char buf[256], *str;
				
		sys = reinterpret_cast<GHOST_System*>(GHOST_ISystem::getSystem());
		wm = sys->getWindowManager();
		windows = wm->getWindows();

		while (str = fgets(buf, 256, m_playfile)) {
			GHOST_IWindow *iwin = NULL;
			GHOST_TEventType type;
			double time;
			int winid, i;
			int ctype;
			
			event = NULL;
			
			if (strcmp(str, "break\n")==0) {
				event = new GHOST_Event(0, GHOST_kEventUnknown, NULL);
				pushEvent(event);
				continue;
			}
			
			sscanf(str, "%lf %d %d", &time, &ctype, &winid);
			type = (GHOST_TEventType)(ctype);
			
			if (lasttime > 0.0) {
				double t = time;
				
				time -= lasttime;
				lasttime = t;
			} else lasttime = time;
			
			for (i=0; i<windows.size(); i++) {
				if (windows[i]->getID() == winid)
					break;
			}
			
			if (i == windows.size()) {
				printf("Eek! Could not find window %d!\n", winid);
				str = fgets(buf, 256, m_playfile);
				continue;
			}
			
			iwin = windows[i];
			
			str = fgets(buf, 256, m_playfile);
			if (!str)
				break;
				
			switch (type) {
				case GHOST_kEventCursorMove:
					event = new GHOST_EventCursor(time*1000, type, iwin, str);
					break;
				case GHOST_kEventButtonDown:
					event = new GHOST_EventButton(time*1000, type, iwin, str);
					break;
				case GHOST_kEventButtonUp:
					event = new GHOST_EventButton(time*1000, type, iwin, str);
					break;
				case GHOST_kEventWheel:
					event = new GHOST_EventWheel(time*1000, type, iwin, str);
					break;
				case GHOST_kEventTrackpad:
					event = new GHOST_EventTrackpad(time*1000, type, iwin, str);
					break;
			
				case GHOST_kEventNDOFMotion:
					break;
				case GHOST_kEventNDOFButton:
					break;
			
				case GHOST_kEventKeyDown:
					event = new GHOST_EventKey(time*1000, type, iwin, str);
					break;
				case GHOST_kEventKeyUp:
					event = new GHOST_EventKey(time*1000, type, iwin, str);
					break;
			//	case GHOST_kEventKeyAuto:
			
				case GHOST_kEventQuit:
					break;
			
				case GHOST_kEventWindowClose:
					break;
				case GHOST_kEventWindowActivate:
					break;
				case GHOST_kEventWindowDeactivate:
					break;
				case GHOST_kEventWindowUpdate:
					break;
				case GHOST_kEventWindowSize:
					break;
				case GHOST_kEventWindowMove:
					break;
				
				case GHOST_kEventDraggingEntered:
					break;
				case GHOST_kEventDraggingUpdated:
					break;
				case GHOST_kEventDraggingExited:
					break;
				case GHOST_kEventDraggingDropDone:
					break;
				
				case GHOST_kEventOpenMainFile:
					break;
			
				case GHOST_kEventTimer:
					break;				
			}
			
			str = fgets(buf, 256, m_playfile);
			if (str) {
				int lshift, rshift, lalt, ralt, lctrl, rctrl, command;
				sscanf(str, "lshift: %d rshift: %d lalt: %d ralt: %d lctrl: %d rctrl: %d command: %d",
				            &lshift, &rshift, &lalt, &ralt, &lctrl, &rctrl, &command);
				modkeys.set(GHOST_kModifierKeyLeftShift, lshift);
				modkeys.set(GHOST_kModifierKeyRightShift, rshift);
				modkeys.set(GHOST_kModifierKeyLeftAlt, lalt);
				modkeys.set(GHOST_kModifierKeyRightAlt, ralt);
				modkeys.set(GHOST_kModifierKeyLeftControl, lctrl);
				modkeys.set(GHOST_kModifierKeyRightControl, rctrl);
				modkeys.set(GHOST_kModifierKeyCommand, command);
			}           
			
			str = fgets(buf, 256, m_playfile);
			if (str) {
				/*read mouse cursor state*/
				sscanf(str, "mcursorstate: %d %d", &x, &y);
			}
			
			if (event) {
				event->setPlaybackCursor(x, y);
				event->setPlaybackModifierKeys(modkeys);
				pushEvent(event);
			}
		}
		
		if (getNumEvents()) {
			handled = true;
			while (getNumEvents()) {
				event = m_events[m_events.size()-1];
				//event->geTime() stores delay between last event and this one
				if (event->getType() == 0 || sys->getMilliSeconds()-m_lasttime < event->getTime()) {
					handled = false;
					
					if (event->getType() == 0) 
						popEvent();		
					break;
				}
				
				//change event->time from delay-since-last-event to 
				//current system timevoid
				m_lasttime = sys->getMilliSeconds();
				event->setTime(m_lasttime);
				
				event->getPlaybackModifierKeys(m_playmods);
				event->getPlaybackCursor(m_x, m_y);
				
				if (!dispatchEvent()) {
					handled = false;
				}
			}
		} else {
			handled = false;
			m_playfile = NULL;
		}
	} else {	
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
