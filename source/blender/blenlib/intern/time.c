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
 */
#include "PIL_time.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32

#include <windows.h>

double PIL_check_seconds_timer(void) 
{
	static int hasperfcounter= -1; /* -1==unknown */
	static double perffreq;

	if (hasperfcounter==-1) {
		__int64 ifreq;
		hasperfcounter= QueryPerformanceFrequency((LARGE_INTEGER*) &ifreq);
		perffreq= (double) ifreq;
	} 

	if (hasperfcounter) {
		__int64 count;

		QueryPerformanceCounter((LARGE_INTEGER*) &count);

		return count/perffreq;
	} else {
		static double accum= 0.0;
		static int ltick= 0;
		int ntick= GetTickCount();

		if (ntick<ltick) {
			accum+= (0xFFFFFFFF-ltick+ntick)/1000.0;
		} else {
			accum+= (ntick-ltick)/1000.0;
		}

		ltick= ntick;
		return accum;
	}
}

void PIL_sleep_ms(int ms)
{
	Sleep(ms);
}

#else

#include <unistd.h>
#include <sys/time.h>

double PIL_check_seconds_timer(void) 
{
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);

	return ((double) tv.tv_sec + tv.tv_usec/1000000.0);
}

void PIL_sleep_ms(int ms)
{
	if (ms>=1000) {
		sleep(ms/1000);
		ms= (ms%1000);
	}
	
	usleep(ms*1000);
}

#endif
