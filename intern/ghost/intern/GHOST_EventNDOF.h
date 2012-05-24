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

/** \file ghost/intern/GHOST_EventNDOF.h
 *  \ingroup GHOST
 * Declaration of GHOST_EventManager class.
 */

#ifndef __GHOST_EVENTNDOF_H__
#define __GHOST_EVENTNDOF_H__

#include "GHOST_Event.h"


class GHOST_EventNDOFMotion : public GHOST_Event
{
protected:
	GHOST_TEventNDOFMotionData m_axisData;

public:
	GHOST_EventNDOFMotion(GHOST_TUns64 time, GHOST_IWindow *window)
		: GHOST_Event(time, GHOST_kEventNDOFMotion, window)
	{
		m_data = &m_axisData;
	}
};


class GHOST_EventNDOFButton : public GHOST_Event
{
protected:
	GHOST_TEventNDOFButtonData m_buttonData;

public:
	GHOST_EventNDOFButton(GHOST_TUns64 time, GHOST_IWindow *window)
		: GHOST_Event(time, GHOST_kEventNDOFButton, window)
	{
		m_data = &m_buttonData;
	}
};

#endif // __GHOST_EVENTNDOF_H__
