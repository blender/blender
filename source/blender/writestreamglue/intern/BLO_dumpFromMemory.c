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
 * streamglue loopback adds a streamGlueHeader to start of the write stream
 */

#include <stdio.h>
#include <stdlib.h>

#include "GEN_messaging.h"
#include "zlib.h"
#include "BLO_writeStreamGlue.h"
#include "BLO_dumpFromMemory.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

	int
BLO_dumpFromMemory(
	unsigned char *data,
	unsigned int dataIn,
	struct streamGlueHeaderStruct *streamGlueHeader)
{
	struct writeStreamGlueStruct *streamGlue = NULL;
	int err = 0;

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_dumpFromMemory: %u streamGlueHeader + %u data = %u\n",
			STREAMGLUEHEADERSIZE,
			dataIn,
			STREAMGLUEHEADERSIZE + dataIn);
#endif
	
	// all data is in. set size in streamGlueHeader and write it out
	streamGlueHeader->totalStreamLength = htonl(dataIn);
	streamGlueHeader->crc = htonl(crc32(0L, (const Bytef *) streamGlueHeader,
										STREAMGLUEHEADERSIZE - 4));
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *)streamGlueHeader,
		STREAMGLUEHEADERSIZE,
		0);
	if (err) return err;

	// write out data
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		data,
		dataIn,
		1);

	return err;
}

