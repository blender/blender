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
 * connect the read stream data processors
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "zlib.h"

#include "GEN_messaging.h"

#include "BLO_readStreamGlue.h"

#include "BLO_readStreamGlueLoopBack.h"
#include "BLO_readfile.h"
#include "BLO_inflate.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

	unsigned int
correctByteOrder(
	unsigned int x)
{
	unsigned char *s = (unsigned char *)&x;
	return (unsigned int)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
}

	int
readStreamGlue(
	void * endControl,
	struct readStreamGlueStruct **control,
	unsigned char *data,
	unsigned int dataIn)
{
	int err = 0;
	
	if (NULL == *control) {
		/* we are called for the first time; play constructor */
		*control = malloc(sizeof (struct readStreamGlueStruct));
		if (*control == NULL) {
			err = BRS_SETFUNCTION(BRS_READSTREAMGLUE) |
				  BRS_SETGENERR(BRS_MALLOC);
			return err;
		}
		(*control)->totalStreamLength = 0;
		(*control)->streamDone = 0;
		(*control)->dataProcessorType = UNKNOWN;
		memset((*control)->headerbuffer, 0, STREAMGLUEHEADERSIZE);
		(*control)->begin = NULL;
		(*control)->process = NULL;
		(*control)->end = NULL;
	}

	/* First check if we have our header filled in yet */
	if ((dataIn > 0) && ((*control)->dataProcessorType == 0)) {
		unsigned int processed;
		processed = ((dataIn + (*control)->streamDone) <= STREAMGLUEHEADERSIZE)
			? dataIn : STREAMGLUEHEADERSIZE;
		memcpy((*control)->headerbuffer + (*control)->streamDone,
			   data, processed);
		(*control)->streamDone += processed;
		dataIn -= processed;
		data += processed;

		if ((*control)->streamDone == STREAMGLUEHEADERSIZE) {
			/* we have the whole header, absorb it */
			struct streamGlueHeaderStruct *header;
			uint32_t crc;
			header = (struct streamGlueHeaderStruct *)
					 (*control)->headerbuffer;
			(*control)->totalStreamLength =
				ntohl(header->totalStreamLength);
			(*control)->dataProcessorType =
				ntohl(header->dataProcessorType);
			crc = crc32(0L, (const Bytef *) header, STREAMGLUEHEADERSIZE - 4);

			if (header->magic == 'A') {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"streamGlue header read. Magic confirmed\n");
#endif
			} else {
				err = BRS_SETFUNCTION(BRS_READSTREAMGLUE) |
					  BRS_SETGENERR(BRS_MAGIC);
				free(*control);
				(*control) = NULL;
				return err;
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"ERROR streamGlue header read. Magic NOT confirmed (%c)\n",
						header->magic);
#endif
			}

			if (crc == ntohl(header->crc)) {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"streamGlue header CRC correct\n");
#endif
			} else {
				err = BRS_SETFUNCTION(BRS_READSTREAMGLUE) |
					  BRS_SETGENERR(BRS_CRCHEADER);
				free(*control);
				(*control) = NULL;
				return err;
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"ERROR streamGlue header CRC NOT correct\n");
#endif
			}

			// No more header-> usage from this point !

#ifndef NDEBUG
			fprintf(GEN_errorstream,
					"read action %d will get %u raw bytes\n",
					(*control)->dataProcessorType,
					(unsigned int) (*control)->totalStreamLength);
#endif
			
			/* Set pointers to the correct dataprocessor functions */
			switch ((*control)->dataProcessorType) {
			case DUMPTOMEMORY:
			case DUMPFROMMEMORY:
				(*control)->begin = blo_readstreamfile_begin;
				(*control)->process = blo_readstreamfile_process;
				(*control)->end = blo_readstreamfile_end;
				break;
			case READBLENFILE:
			case WRITEBLENFILE:
				(*control)->begin = readStreamGlueLoopBack_begin;
				(*control)->process = readStreamGlueLoopBack_process;
				(*control)->end = readStreamGlueLoopBack_end;
				break;
			case INFLATE:
			case DEFLATE:
				(*control)->begin = BLO_inflate_begin;
				(*control)->process = BLO_inflate_process;
				(*control)->end = BLO_inflate_end;
				break;
			default:
				err = BRS_SETFUNCTION(BRS_READSTREAMGLUE) |
					  BRS_SETSPECERR(BRS_UNKNOWN);
				(*control) = NULL;
				free(*control);
				return err;
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"unknown dataProcessorType %d\n",
						(*control)->dataProcessorType);
#endif
				break;
			}
			/* Call the dataprocessors begin() and
			 * store its (*control) struct pointer */
			(*control)->ProcessorTypeControlStruct =
				(*(*control)->begin)(endControl);
			if ((*control)->ProcessorTypeControlStruct == NULL) {
				free(*control);
				(*control) = NULL;
				return err;
			}
		}

	}

	/* Is there really (still) new data available ? */
	if (dataIn > 0) {
		err = (*(*control)->process)((*control)->ProcessorTypeControlStruct,
									 data, dataIn);
		if (err) {
			free(*control);
			(*control) = NULL;
			return err;
		}
		(*control)->streamDone += dataIn;
	}
	if ((*control)->streamDone == (*control)->totalStreamLength +
			STREAMGLUEHEADERSIZE) {
		err = (*(*control)->end)((*control)->ProcessorTypeControlStruct);
		free(*control);
		(*control) = NULL;
	}
	return err;
}

