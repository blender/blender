/**
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
 * 
 * Just the functions to maintain a central event
 * queue.
 */

#include <stdlib.h>
#include <string.h>
#include "BIF_mainqueue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct {
	unsigned short event;
	short val;
	char ascii;
} QEvent;

static QEvent mainqueue[MAXQUEUE];
static unsigned int nevents= 0;

unsigned short mainqread(short *val, char *ascii)
{
	if (nevents) {
		nevents--;
		
		*val= mainqueue[nevents].val;
		*ascii= mainqueue[nevents].ascii;
		return mainqueue[nevents].event;
	} else
		return 0;
}

void mainqenter(unsigned short event, short val)
{
	mainqenter_ext(event, val, 0);
}

void mainqenter_ext(unsigned short event, short val, char ascii)
{
	if (!event)
		return;

	if (nevents<MAXQUEUE) {
		memmove(mainqueue+1, mainqueue, sizeof(*mainqueue)*nevents);
		
		mainqueue[0].event= event;
		mainqueue[0].val= val;
		mainqueue[0].ascii= ascii;
		
		nevents++;
	}
}

void mainqpushback(unsigned short event, short val, char ascii)
{
	if (nevents<MAXQUEUE) {
		mainqueue[nevents].event= event;
		mainqueue[nevents].val= val;
		mainqueue[nevents].ascii= ascii;
		nevents++;
	}
}

unsigned short mainqtest()
{
	if (nevents)
		return mainqueue[nevents-1].event;
	else
		return 0;
}
