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

/** \file ghost/intern/GHOST_Event.h
 *  \ingroup GHOST
 * Declaration of GHOST_Event class.
 */

#ifndef __GHOST_EVENT_H__
#define __GHOST_EVENT_H__

#include "GHOST_IEvent.h"


/**
 * Base class for events received the operating system.
 * @author	Maarten Gribnau
 * @date	May 11, 2001
 */
class GHOST_Event : public GHOST_IEvent
{
public:
	/**
	 * Constructor.
	 * @param msec	The time this event was generated.
	 * @param type	The type of this event.
	 * @param window The generating window (or NULL if system event).
	 */
	GHOST_Event(GHOST_TUns64 msec, GHOST_TEventType type, GHOST_IWindow *window)
		: m_type(type), m_time(msec), m_window(window), m_data(0)
	{
	}

	/**
	 * Returns the event type.
	 * @return The event type.
	 */
	virtual GHOST_TEventType getType()
	{ 
		return m_type;
	}

	/**
	 * Returns the time this event was generated.
	 * @return The event generation time.
	 */
	virtual GHOST_TUns64 getTime()
	{
		return m_time;
	}

	/**
	 * Returns the window this event was generated on, 
	 * or NULL if it is a 'system' event.
	 * @return The generating window.
	 */
	virtual GHOST_IWindow *getWindow()
	{
		return m_window;
	}

	/**
	 * Returns the event data.
	 * @return The event data.
	 */
	virtual GHOST_TEventDataPtr getData()
	{
		return m_data;
	}

protected:
	/** Type of this event. */
	GHOST_TEventType m_type;
	/** The time this event was generated. */
	GHOST_TUns64 m_time;
	/** Pointer to the generating window. */
	GHOST_IWindow *m_window;
	/** Pointer to the event data. */
	GHOST_TEventDataPtr m_data;
};

#endif // __GHOST_EVENT_H__

