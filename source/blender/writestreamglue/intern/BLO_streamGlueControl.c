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
 * define what actions a write stream should do 
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "BLO_writeStreamGlue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

	struct streamGlueControlStruct *
streamGlueControlConstructor(
	void)
{
	struct streamGlueControlStruct *control;
	control = malloc(sizeof(struct streamGlueControlStruct));
	assert(control);
	// TODO handle malloc errors
	control->actions = 0;
	control->actionsDone = 0;
	memset(control->action, 0, MAXSTREAMLENGTH);
	return(control);
}

	void
streamGlueControlDestructor(
	struct streamGlueControlStruct *streamControl)
{
	free(streamControl);
}

	int
streamGlueControlAppendAction(
	struct streamGlueControlStruct *streamControl,
	unsigned char nextAction)
{
	assert(streamControl);
	assert(streamControl->actions < MAXSTREAMLENGTH);
	streamControl->action[streamControl->actions] = nextAction;
	streamControl->actions++;
	return(streamControl->actions);
}

	unsigned char
streamGlueControlGetNextAction(
	struct streamGlueControlStruct *streamControl)
{
	unsigned char nextAction;
	assert(streamControl);
	assert(streamControl->actionsDone < streamControl->actions);
	if (streamControl->actionsDone >= streamControl->actions) {
		// the stream should have been terminated by a data
		// processor, but instead streamGlue is called again ...
		nextAction = UNKNOWN;	// best guess ...
	} else {
		nextAction = streamControl->action[streamControl->actionsDone];
		streamControl->actionsDone++;
	}
	return(nextAction);
}

