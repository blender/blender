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

/** \file ghost/intern/GHOST_EventManager.h
 *  \ingroup GHOST
 * Declaration of GHOST_EventManager class.
 */

#ifndef __GHOST_EVENTMANAGER_H__
#define __GHOST_EVENTMANAGER_H__

#include <deque>
#include <vector>

#include "GHOST_IEventConsumer.h"


/**
 * Manages an event stack and a list of event consumers.
 * The stack works on a FIFO (First In First Out) basis.
 * Events are pushed on the front of the stack and retrieved from the back.
 * Ownership of the event is transferred to the event manager as soon as an event is pushed.
 * Ownership of the event is transferred from the event manager as soon as an event is popped.
 * Events can be dispatched to the event consumers.
 */
class GHOST_EventManager
{
public:
	/**
	 * Constructor.
	 */
	GHOST_EventManager();

	/**
	 * Destructor.
	 */
	virtual ~GHOST_EventManager();

	/**
	 * Returns the number of events currently on the stack.
	 * @return The number of events on the stack.
	 */
	virtual GHOST_TUns32 getNumEvents();

	/**
	 * Returns the number of events of a certain type currently on the stack.
	 * @param type The type of events to be counted.
	 * @return The number of events on the stack of this type.
	 */
	virtual GHOST_TUns32 getNumEvents(GHOST_TEventType type);

	/**
	 * Return the event at the top of the stack without removal.
	 * Do not delete the event!
	 * @return The event at the top of the stack.
	 */
	virtual GHOST_IEvent *peekEvent();

	/**
	 * Pushes an event on the stack.
	 * To dispatch it, call dispatchEvent() or dispatchEvents().
	 * Do not delete the event!
	 * @param event	The event to push on the stack.
	 */
	virtual GHOST_TSuccess pushEvent(GHOST_IEvent *event);

	/**
	 * Dispatches the given event directly, bypassing the event stack.
	 * @return Indication as to whether any of the consumers handled the event.
	 */
	virtual bool dispatchEvent(GHOST_IEvent *event);

	/**
	 * Dispatches the event at the back of the stack.
	 * The event will be removed from the stack.
	 * @return Indication as to whether any of the consumers handled the event.
	 */
	virtual bool dispatchEvent();

	/**
	 * Dispatches all the events on the stack.
	 * The event stack will be empty afterwards.
	 * @return Indication as to whether any of the consumers handled the events.
	 */
	virtual bool dispatchEvents();

	/**
	 * Adds a consumer to the list of event consumers.
	 * @param consumer The consumer added to the list.
	 * @return Indication as to whether addition has succeeded.
	 */
	virtual GHOST_TSuccess addConsumer(GHOST_IEventConsumer *consumer);

	/**
	 * Removes a consumer from the list of event consumers.
	 * @param consumer The consumer removed from the list.
	 * @return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess removeConsumer(GHOST_IEventConsumer *consumer);

	/**
	 * Removes all events for a window from the stack.
	 * @param	window	The window to remove events for.
	 */
	virtual void
	removeWindowEvents(
	    GHOST_IWindow *window
	    );

	/**
	 * Removes all events of a certain type from the stack.
	 * The window parameter is optional. If non-null, the routine will remove
	 * events only associated with that window.
	 * @param	type	The type of events to be removed.
	 * @param	window	The window to remove the events for.
	 */
	virtual void
	removeTypeEvents(
	    GHOST_TEventType type,
	    GHOST_IWindow *window = 0
	    );

protected:
	/**
	 * Returns the event at the top of the stack and removes it.
	 * Delete the event after use!
	 * @return The event at the top of the stack.
	 */
	virtual GHOST_IEvent *popEvent();

	/**
	 * Removes all events from the stack.
	 */
	virtual void disposeEvents();

	/** A stack with events. */
	typedef std::deque<GHOST_IEvent *> TEventStack;
	
	/** The event stack. */
	std::deque<GHOST_IEvent *> m_events;

	/** A vector with event consumers. */
	typedef std::vector<GHOST_IEventConsumer *> TConsumerVector;

	/** The list with event consumers. */
	TConsumerVector m_consumers;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_EventManager")
#endif
};

#endif // __GHOST_EVENTMANAGER_H__

