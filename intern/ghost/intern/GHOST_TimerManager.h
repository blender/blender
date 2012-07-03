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

/** \file ghost/intern/GHOST_TimerManager.h
 *  \ingroup GHOST
 * Declaration of GHOST_TimerManager class.
 */

#ifndef __GHOST_TIMERMANAGER_H__
#define __GHOST_TIMERMANAGER_H__

#include <vector>

#include "GHOST_Types.h"

class GHOST_TimerTask;


/**
 * Manages a list of timer tasks.
 * Timer tasks added are owned by the manager.
 * Don't delete timer task objects.
 * @author	Maarten Gribnau
 * @date	May 31, 2001
 */
class GHOST_TimerManager
{
public:
	/**
	 * Constructor.
	 */
	GHOST_TimerManager();

	/**
	 * Destructor.
	 */
	virtual ~GHOST_TimerManager();

	/**
	 * Returns the number of timer tasks.
	 * @return The number of events on the stack.
	 */
	virtual GHOST_TUns32 getNumTimers();

	/**
	 * Returns whther this timer task ins in our list.
	 * @return Indication of presence.
	 */
	virtual bool getTimerFound(GHOST_TimerTask *timer);

	/**
	 * Adds a timer task to the list.
	 * It is only added when it not already present in the list.
	 * @param timer The timer task added to the list.
	 * @return Indication as to whether addition has succeeded.
	 */
	virtual GHOST_TSuccess addTimer(GHOST_TimerTask *timer);

	/**
	 * Removes a timer task from the list.
	 * It is only removed when it is found in the list.
	 * @param timer The timer task to be removed from the list.
	 * @return Indication as to whether removal has succeeded.
	 */
	virtual GHOST_TSuccess removeTimer(GHOST_TimerTask *timer);

	/**
	 * Finds the soonest time the next timer would fire.
	 * @return The soonest time the next timer would fire, 
	 * or GHOST_kFireTimeNever if no timers exist.
	 */
	virtual GHOST_TUns64 nextFireTime();
	
	/**
	 * Checks all timer tasks to see if they are expired and fires them if needed.
	 * @param time The current time.
	 * @return True if any timers were fired.
	 */
	virtual bool fireTimers(GHOST_TUns64 time);

	/**
	 * Checks this timer task to see if they are expired and fires them if needed.
	 * @param time The current time.
	 * @param task The timer task to check and optionally fire.
	 * @return True if the timer fired.
	 */
	virtual bool fireTimer(GHOST_TUns64 time, GHOST_TimerTask *task);

protected:
	/**
	 * Deletes all timers.
	 */
	void disposeTimers();

	typedef std::vector<GHOST_TimerTask *> TTimerVector;
	/** The list with event consumers. */
	TTimerVector m_timers;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_TimerManager")
#endif
};

#endif // __GHOST_TIMERMANAGER_H__

