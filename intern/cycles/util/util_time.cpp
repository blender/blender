/*
 * Copyright 2011, Blender Foundation.
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
 */

#include <stdlib.h>

#include "util_time.h"

#ifdef _WIN32

#include <windows.h>

CCL_NAMESPACE_BEGIN

double time_dt()
{
	__int64 frequency, counter;

	QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
	QueryPerformanceCounter((LARGE_INTEGER*)&counter);

	return (double)counter/(double)frequency;
}

void time_sleep(double t)
{
	Sleep((int)(t*1000));
}

CCL_NAMESPACE_END

#else

#include <sys/time.h>
#include <unistd.h>

CCL_NAMESPACE_BEGIN

double time_dt()
{
	struct timeval now;
	gettimeofday(&now, NULL);

	return now.tv_sec + now.tv_usec*1e-6;
}

/* sleep t seconds */
void time_sleep(double t)
{
	/* get whole seconds */
	int s = (int)t;

	if(s >= 1) {
		sleep(s);

		/* adjust parameter to remove whole seconds */
		t -= s;
	}

	/* get microseconds */
	int us = (int)(t * 1e6);
	if (us > 0)
		usleep(us);
}

CCL_NAMESPACE_END

#endif

