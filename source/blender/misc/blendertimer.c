/*
 * blendertimer.c
 * 
 * A system-independent timer
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef WIN32
#include "BLI_winstuff.h"
#else
#include <unistd.h>
#include <sys/times.h>
#include <sys/time.h>	/* struct timeval */
#include <sys/resource.h>	/* struct rusage */
#endif

#include "BLI_blenlib.h"
#include "BKE_global.h"
#include "BIF_screen.h" /* qtest(), extern_qread() */
#include "mydevice.h"

#include "blendertimer.h"

#include "PIL_time.h"

int MISC_test_break(void)
{
	if (!G.background) {
		static double ltime= 0;
		double curtime= PIL_check_seconds_timer();

			/* only check for breaks every 10 milliseconds
			 * if we get called more often.
			 */
		if ((curtime-ltime)>.001) {
			ltime= curtime;

			while(qtest()) {
				short val;
				if (extern_qread(&val) == ESCKEY) {
					G.afbreek= 1;
				}
			}
		}
	}

	return (G.afbreek==1);
}
