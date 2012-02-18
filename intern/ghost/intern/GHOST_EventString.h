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

/** \file ghost/intern/GHOST_EventString.h
 *  \ingroup GHOST
 * Declaration of GHOST_EventString class.
 */

#ifndef __GHOST_EVENTSTRING_H__
#define __GHOST_EVENTSTRING_H__

#include "GHOST_Event.h"


/**
 * Generic class for events with string data
 * @author	Damien Plisson
 * @date	Feb 1, 2010
 */
class GHOST_EventString : public GHOST_Event
{
public:
	/**
	 * Constructor.
	 * @param msec	The time this event was generated.
	 * @param type	The type of this event.
	 * @param window The generating window (or NULL if system event).
	 * @param data_ptr Pointer to the (unformatted) data associated with the event
	 */
	GHOST_EventString(GHOST_TUns64 msec, GHOST_TEventType type, GHOST_IWindow* window, GHOST_TEventDataPtr data_ptr)
		: GHOST_Event(msec, type, window)	{
			m_data = data_ptr;
	}

	~GHOST_EventString()
	{
		if (m_data) free(m_data);
	}
};

#endif // __GHOST_EVENTSTRING_H__

