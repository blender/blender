/**
 * $Id$
 *
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
 * Timing routine taken and modified from KX_BlenderSystem.cpp
 */

#include <windows.h>
#include "GPW_System.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GPW_System::GPW_System(void)
{
	m_freq = 0;
	m_lastCount = 0;
	m_lastRest = 0;
	m_lastTime = 0;
}


double GPW_System::GetTimeInSeconds()
{
#if 0
	double secs = ::GetTickCount();
	secs /= 1000.;
	return secs;
#else

	// 03/20/1999 Thomas Hieber: completely redone to get true Millisecond 
	// accuracy instead of very rough ticks. This routine will also provide
	// correct wrap around at the end of "long"

	// m_freq was set to -1, if the current Hardware does not support 
	// high resolution timers. We will use GetTickCount instead then.
	if (m_freq < 0) {
		return ::GetTickCount();
	}

	// m_freq is 0, the first time this function is being called.
	if (m_freq == 0) {
		// Try to determine the frequency of the high resulution timer
		if (!::QueryPerformanceFrequency((LARGE_INTEGER*)&m_freq)) {
			// There is no such timer....
			m_freq = -1;
			return 0;
		}
	}

	// Retrieve current count
	__int64 count = 0;
	::QueryPerformanceCounter((LARGE_INTEGER*)&count);

	// Calculate the time passed since last call, and add the rest of
	// those tics that didn't make it into the last reported time.
	__int64 delta = 1000*(count-m_lastCount) + m_lastRest;

	m_lastTime += (long)(delta/m_freq);	// Save the new value
	m_lastRest  = delta%m_freq;			// Save those ticks not being counted
	m_lastCount = count;				// Save last count

	// Return a high quality measurement of time
	return m_lastTime/1000.0; 
#endif
}


