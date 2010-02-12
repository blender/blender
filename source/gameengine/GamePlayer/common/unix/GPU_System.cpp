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
   
#include <sys/time.h>
#include "GPU_System.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static struct timeval startTime;
static int startTimeDone = 0;


double GPU_System::GetTimeInSeconds()
{
	if(!startTimeDone)
	{
		gettimeofday(&startTime, NULL);
		startTimeDone = 1;
	}

	struct timeval now;
	gettimeofday(&now, NULL);
	// next '1000' are used for precision
	long ticks = (now.tv_sec - startTime.tv_sec) * 1000 + (now.tv_usec - startTime.tv_usec) / 1000;
	double secs = (double)ticks / 1000.0;
	return secs;
}
