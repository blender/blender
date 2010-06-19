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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 

#ifndef _GHOST_EVENT_NDOF_H_
#define _GHOST_EVENT_NDOF_H_

#include "GHOST_Event.h"

/**
 * N-degree of freedom device event.
 */
class GHOST_EventNDOF : public GHOST_Event
{
public:
	/**
	 * Constructor.
	 * @param msec		The time this event was generated.
	 * @param type		The type of this event.
	 * @param x			The x-coordinate of the location the cursor was at at the time of the event.
	 * @param y			The y-coordinate of the location the cursor was at at the time of the event.
	 */
	GHOST_EventNDOF(GHOST_TUns64 msec, GHOST_TEventType type, GHOST_IWindow* window, 
        GHOST_TEventNDOFData data)
		: GHOST_Event(msec, type, window)
	{
		m_ndofEventData = data;
		m_data = &m_ndofEventData;
	}

protected:
	/** translation & rotation from the device. */
	GHOST_TEventNDOFData m_ndofEventData;
};


#endif // _GHOST_EVENT_NDOF_H_

