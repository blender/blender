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
 * connect the data stream processors
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "zlib.h"
#include "GEN_messaging.h"
#include "BLO_writeStreamGlue.h"
#include "BLO_dumpFromMemory.h"
#include "BLO_writeblenfile.h"
#include "BLO_deflate.h"
#include "BLO_encrypt.h"
#include "BLO_sign.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * streamGlueWrite does not really stream; it buffers all data it gets
 * because it needs everything to create the header, which is in front
 * of the data (to make reading easier, which occurs much more often
 * than writing and is thus more important to optimize for).
 * @param streamControl contains a list of Glue actions. Every
 *        streamGlueWrite constructor eats up the next first action.
 */
	int
writeStreamGlue(
	struct streamGlueControlStruct *streamGlueControl,
	struct writeStreamGlueStruct **streamGlue,
	unsigned char *data,
	unsigned int dataIn,
	int finishUp)
{
	int err = 0;
	
	if (NULL == *streamGlue) {
		/* we are called for the first time; play constructor */
		(*streamGlue) = malloc(sizeof(struct writeStreamGlueStruct));

		if (!(*streamGlue)) {
			err = BWS_SETFUNCTION(BWS_WRITESTREAMGLUE) |
				  BWS_SETGENERR(BWS_MALLOC);
			return err;
		}

		(*streamGlue)->dataProcessorType =
			streamGlueControlGetNextAction(streamGlueControl);
		(*streamGlue)->streamBufferCount = 0;
		(*streamGlue)->streamBuffer = NULL;
	}
	if (dataIn > 0) {
		/* simply buffer it */
		(*streamGlue)->streamBuffer = realloc((*streamGlue)->streamBuffer,
				   	dataIn + (*streamGlue)->streamBufferCount);
		if (!(*streamGlue)->streamBuffer) {
			err = BWS_SETFUNCTION(BWS_WRITESTREAMGLUE) |
				  BWS_SETGENERR(BWS_MALLOC);
			free(*streamGlue);
			return err;
		}
		memcpy((*streamGlue)->streamBuffer + (*streamGlue)->streamBufferCount,
			   data, dataIn);
		(*streamGlue)->streamBufferCount += dataIn;
	}
	if (finishUp) {
		/* all data is in, create header and call data processor */

		/* first create the streamGlueHeaderStruct */
		struct streamGlueHeaderStruct *streamGlueHeader;
		streamGlueHeader = malloc(STREAMGLUEHEADERSIZE);
		if (!streamGlueHeader) {
			err = BWS_SETFUNCTION(BWS_WRITESTREAMGLUE) |
				  BWS_SETGENERR(BWS_MALLOC);
			free((*streamGlue)->streamBuffer);
			free(*streamGlue);
			return err;
		}
		streamGlueHeader->magic = 'A';
		streamGlueHeader->totalStreamLength = 0; // set in the actions _end
		streamGlueHeader->dataProcessorType =
			htonl((*streamGlue)->dataProcessorType);
		streamGlueHeader->crc = 0; // set in in the actions _end

#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"streamGlue: write %d gets %u data + %u streamGlueHeader = %u\n",
				(*streamGlue)->dataProcessorType,
				(*streamGlue)->streamBufferCount,
				STREAMGLUEHEADERSIZE,
				(*streamGlue)->streamBufferCount + STREAMGLUEHEADERSIZE);
#endif

		/* all data ready, start the right data processor */
		switch ((*streamGlue)->dataProcessorType) {
		case DUMPFROMMEMORY:
			err = BLO_dumpFromMemory((*streamGlue)->streamBuffer,
									 (*streamGlue)->streamBufferCount,
									 streamGlueHeader);
			break;
		case DEFLATE:
			err = BLO_deflate((*streamGlue)->streamBuffer,
							  (*streamGlue)->streamBufferCount,
							  streamGlueHeader);
			break;
		case ENCRYPT:
			err = BLO_encrypt((*streamGlue)->streamBuffer,
							  (*streamGlue)->streamBufferCount,
							  streamGlueHeader);
			break;
		case SIGN:
			err = BLO_sign((*streamGlue)->streamBuffer,
						   (*streamGlue)->streamBufferCount,
						   streamGlueHeader);
			break;
		case WRITEBLENFILE:
			err = BLO_writeblenfile((*streamGlue)->streamBuffer,
									(*streamGlue)->streamBufferCount,
									streamGlueHeader);
			break;
		default:
#ifndef NDEBUG
			fprintf(GEN_errorstream,
					"unknown dataProcessorType %d\n",
					(*streamGlue)->dataProcessorType);
#endif
			err = BWS_SETFUNCTION(BWS_WRITESTREAMGLUE) |
				  BWS_SETSPECERR(BWS_UNKNOWN);
			break;
		}

		free(streamGlueHeader);
		free((*streamGlue)->streamBuffer);
		free(*streamGlue);
	}
	return err;
}

