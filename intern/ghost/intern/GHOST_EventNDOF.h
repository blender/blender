/**
 * $Id: GHOST_EventNdof.h,v 1.6 2002/12/28 22:26:45 maarten Exp $
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
/**
 * @file	GHOST_EventNdof.h
 * Declaration of GHOST_EventNdof class.
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

