/**
 * @file PIL_time.h
 * 
 * Platform independant time functions.
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
 
#ifndef PIL_TIME_H
#define PIL_TIME_H

#ifdef __cplusplus
extern "C" { 
#endif

extern 
	/** Return an indication of time, expressed	as
	 * seconds since some fixed point. Successive calls
	 * are guarenteed to generate values greator than or 
	 * equal to the last call.
	 */
double	PIL_check_seconds_timer		(void);

	/**
	 * Platform-independant sleep function.
	 * @param ms Number of milliseconds to sleep
	 */
void	PIL_sleep_ms				(int ms);

#ifdef __cplusplus
}
#endif

#endif

