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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
 * @file	GHOST_EventWheel.h
 * Declaration of GHOST_EventWheel class.
 */

#ifndef _GHOST_EVENT_WHEEL_H_
#define _GHOST_EVENT_WHEEL_H_

#include "GHOST_Event.h"

/**
 * Mouse wheel event.
 * The displacement of the mouse wheel is counted in ticks.
 * A positive value means the wheel is turned away from the user.
 * @author	Maarten Gribnau
 * @date	May 11, 2001
 */
class GHOST_EventWheel : public GHOST_Event
{
public:
	/**
	 * Constructor.
	 * @param msec		The time this event was generated.
	 * @param type		The type of this event.
	 * @param z			The displacement of the mouse wheel.
	 */
	GHOST_EventWheel(GHOST_TUns64 msec, GHOST_IWindow* window, GHOST_TInt32 z)
		: GHOST_Event(msec, GHOST_kEventWheel, window)
	{
		m_wheelEventData.z = z;
		m_data = &m_wheelEventData;
	}

protected:
	/** The z-displacement of the mouse wheel. */
	GHOST_TEventWheelData m_wheelEventData;
};


#endif // _GHOST_EVENT_WHEEL_H_

