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
 * struct/function that connects the data stream processors
 */

#ifndef BLO_WRITESTREAMGLUE_H
#define BLO_WRITESTREAMGLUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BLO_readStreamGlue.h"
#include "BLO_writeStreamErrors.h"

/******************** start BLO_streamGlueControl.c part *****************/
struct streamGlueControlStruct {
	int actions;
	int actionsDone;
	unsigned char action[MAXSTREAMLENGTH];
};

	struct streamGlueControlStruct *
streamGlueControlConstructor(
	void);

	void
streamGlueControlDestructor(
	struct streamGlueControlStruct *streamControl);

	int
streamGlueControlAppendAction(
	struct streamGlueControlStruct *streamControl,
	unsigned char nextAction);

	unsigned char
streamGlueControlGetNextAction(
	struct streamGlueControlStruct *streamControl);

// TODO avoid this global variable
extern struct streamGlueControlStruct *Global_streamGlueControl;
/******************** end BLO_streamGlueControl.c part *****************/

struct writeStreamGlueStruct {
	int dataProcessorType;
	unsigned int streamBufferCount;
	unsigned char *streamBuffer;
};

	int
writeStreamGlue(
	struct streamGlueControlStruct *streamGlueControl,
	struct writeStreamGlueStruct **streamGlue,
	unsigned char *data,
	unsigned int dataIn,
	int finishUp);

#ifdef __cplusplus
}
#endif

#endif /* BLO_WRITESTREAMGLUE_H */

