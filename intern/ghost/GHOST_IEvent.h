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

/** \file ghost/GHOST_IEvent.h
 *  \ingroup GHOST
 * Declaration of GHOST_IEvent interface class.
 */

#ifndef __GHOST_IEVENT_H__
#define __GHOST_IEVENT_H__

#include <stddef.h>
#include "GHOST_Types.h"

class GHOST_IWindow;

/**
 * Interface class for events received from GHOST.
 * You should not need to inherit this class. The system will pass these events
 * to the GHOST_IEventConsumer::processEvent() method of event consumers.<br>
 * Use the getType() method to retrieve the type of event and the getData() 
 * method to get the event data out. Using the event type you can cast the 
 * event data to the correct event dat structure.
 * \see GHOST_IEventConsumer#processEvent
 * \see GHOST_TEventType
 * \author	Maarten Gribnau
 * \date	May 31, 2001
 */
class GHOST_IEvent
{
public:
	/**
	 * Destructor.
	 */
	virtual ~GHOST_IEvent()
	{
	}

	/**
	 * Returns the event type.
	 * \return The event type.
	 */
	virtual GHOST_TEventType getType() = 0;

	/**
	 * Returns the time this event was generated.
	 * \return The event generation time.
	 */
	virtual GHOST_TUns64 getTime() = 0;

	/**
	 * Returns the window this event was generated on, 
	 * or NULL if it is a 'system' event.
	 * \return The generating window.
	 */
	virtual GHOST_IWindow *getWindow() = 0;
	
	/**
	 * Returns the event data.
	 * \return The event data.
	 */
	virtual GHOST_TEventDataPtr getData() = 0;
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_IEvent")
#endif
};

#endif // __GHOST_IEVENT_H__

