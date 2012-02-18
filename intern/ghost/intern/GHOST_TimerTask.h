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

/** \file ghost/intern/GHOST_TimerTask.h
 *  \ingroup GHOST
 * Declaration of GHOST_TimerTask class.
 */

#ifndef __GHOST_TIMERTASK_H__
#define __GHOST_TIMERTASK_H__

#include "GHOST_ITimerTask.h"


/**
 * Implementation of a timer task.
 * @author	Maarten Gribnau
 * @date	May 28, 2001
 */
class GHOST_TimerTask : public GHOST_ITimerTask
{
public:
	/**
	 * Constructor.
	 * @param start		The timer start time.
	 * @param interval	The interval between calls to the timerProc
	 * @param timerProc	The callbak invoked when the interval expires.
	 * @param data		The timer user data.
	 */
	GHOST_TimerTask(GHOST_TUns64 start,
	                GHOST_TUns64 interval,
	                GHOST_TimerProcPtr timerProc,
	                GHOST_TUserDataPtr userData = 0)
		: m_start(start),
	      m_interval(interval),
	      m_next(start),
	      m_timerProc(timerProc),
	      m_userData(userData),
	      m_auxData(0)
	{
	}

	/**
	 * Returns the timer start time.
	 * @return The timer start time.
	 */
	inline virtual GHOST_TUns64 getStart() const
	{
		return m_start;
	}

	/**
	 * Changes the timer start time.
	 * @param start The timer start time.
	 */
	virtual void setStart(GHOST_TUns64 start)
	{ 
		m_start = start;
	}

	/**
	 * Returns the timer interval.
	 * @return The timer interval.
	 */
	inline virtual GHOST_TUns64 getInterval() const
	{
		return m_interval;
	}

	/**
	 * Changes the timer interval.
	 * @param interval The timer interval.
	 */
	virtual void setInterval(GHOST_TUns64 interval)
	{ 
		m_interval = interval;
	}

	/**
	 * Returns the time the timerProc will be called.
	 * @return The time the timerProc will be called.
	 */
	inline virtual GHOST_TUns64 getNext() const
	{
		return m_next;
	}

	/**
	 * Changes the time the timerProc will be called.
	 * @param next The time the timerProc will be called.
	 */
	virtual void setNext(GHOST_TUns64 next)
	{ 
		m_next = next;
	}

	/**
	 * Returns the timer callback.
	 * @return the timer callback.
	 */
	inline virtual GHOST_TimerProcPtr getTimerProc() const
	{
		return m_timerProc;
	}

	/**
	 * Changes the timer callback.
	 * @param The timer callback.
	 */
	inline virtual void setTimerProc(const GHOST_TimerProcPtr timerProc)
	{
		m_timerProc = timerProc;
	}

	/**
	 * Returns the timer user data.
	 * @return The timer user data.
	 */
	inline virtual GHOST_TUserDataPtr getUserData() const
	{
		return m_userData;
	}
	
	/**
	 * Changes the time user data.
	 * @param data The timer user data.
	 */
	virtual void setUserData(const GHOST_TUserDataPtr userData)
	{
		m_userData = userData;
	}

	/**
	 * Returns the auxiliary storage room.
	 * @return The auxiliary storage room.
	 */
	inline virtual GHOST_TUns32 getAuxData() const
	{
		return m_auxData;
	}

	/**
	 * Changes the auxiliary storage room.
	 * @param auxData The auxiliary storage room.
	 */
	virtual void setAuxData(GHOST_TUns32 auxData)
	{ 
		m_auxData = auxData;
	}

protected:
	/** The time the timer task was started. */
	GHOST_TUns64 m_start;

	/** The interval between calls. */
	GHOST_TUns64 m_interval;

	/** The time the timerProc will be called. */
	GHOST_TUns64 m_next;

	/** The callback invoked when the timer expires. */
	GHOST_TimerProcPtr m_timerProc;

	/** The timer task user data. */
	GHOST_TUserDataPtr m_userData;

	/** Auxiliary storage room. */
	GHOST_TUns32 m_auxData;
};

#endif // __GHOST_TIMERTASK_H__

