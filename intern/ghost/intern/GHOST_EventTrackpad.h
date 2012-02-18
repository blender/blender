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
 * Contributor(s):  James Deery		11/2009
 *					Damien Plisson	12/2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_EventTrackpad.h
 *  \ingroup GHOST
 * Declaration of GHOST_EventTrackpad class.
 */

#ifndef __GHOST_EVENTTRACKPAD_H__
#define __GHOST_EVENTTRACKPAD_H__

#include "GHOST_Event.h"

/**
 * Trackpad (scroll, magnify, rotate, ...) event.
 */
class GHOST_EventTrackpad : public GHOST_Event
{
public:
	/**
	 * Constructor.
	 * @param msec		The time this event was generated.
	 * @param type		The type of this event.
	 * @param subtype	The subtype of the event.
	 * @param x			The x-delta of the pan event.
	 * @param y			The y-delta of the pan event.
	 */
	GHOST_EventTrackpad(GHOST_TUns64 msec,
	                    GHOST_IWindow* window,
	                    GHOST_TTrackpadEventSubTypes subtype,
	                    GHOST_TInt32 x, GHOST_TInt32 y,
	                    GHOST_TInt32 deltaX, GHOST_TInt32 deltaY)
		: GHOST_Event(msec, GHOST_kEventTrackpad, window)
	{
		m_trackpadEventData.subtype = subtype;
		m_trackpadEventData.x = x;
		m_trackpadEventData.y = y;
		m_trackpadEventData.deltaX = deltaX;
		m_trackpadEventData.deltaY = deltaY;
		m_data = &m_trackpadEventData;
	}

protected:
	/** The mouse pan data */
	GHOST_TEventTrackpadData m_trackpadEventData;
};


#endif // _GHOST_EVENT_PAN_H_

