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

/** \file ghost/GHOST_ITimerTask.h
 *  \ingroup GHOST
 * Declaration of GHOST_ITimerTask interface class.
 */

#ifndef __GHOST_ITIMERTASK_H__
#define __GHOST_ITIMERTASK_H__

#include "GHOST_Types.h"


/**
 * Interface for a timer task.
 * Timer tasks are created by the system and can be installed by the system.
 * After installation, the timer callback-procedure or "timerProc" will be called 
 * periodically. You should not need to inherit this class. It is passed to the
 * application in the timer-callback.<br>
 * <br>
 * Note that GHOST processes timers in the UI thread. You should ask GHOST 
 * process messages in order for the timer-callbacks to be called.
 * @see GHOST_ISystem#installTimer
 * @see GHOST_TimerProcPtr
 * @author	Maarten Gribnau
 * @date	May 31, 2001
 */
class GHOST_ITimerTask
{
public:
	/**
	 * Destructor.
	 */
	virtual ~GHOST_ITimerTask()
	{
	}

	/**
	 * Returns the timer callback.
	 * @return The timer callback.
	 */
	virtual GHOST_TimerProcPtr getTimerProc() const = 0;

	/**
	 * Changes the timer callback.
	 * @param timerProc The timer callback.
	 */
	virtual void setTimerProc(const GHOST_TimerProcPtr timerProc) = 0;

	/**
	 * Returns the timer user data.
	 * @return The timer user data.
	 */
	virtual GHOST_TUserDataPtr getUserData() const = 0;
	
	/**
	 * Changes the time user data.
	 * @param data The timer user data.
	 */
	virtual void setUserData(const GHOST_TUserDataPtr userData) = 0;

#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GHOST:GHOST_ITimerTask"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // __GHOST_ITIMERTASK_H__

