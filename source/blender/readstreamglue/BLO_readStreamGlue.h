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

#ifndef BLO_READSTREAMGLUE_H
#define BLO_READSTREAMGLUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "BLO_sys_types.h"
#include "BLO_readStreamErrors.h"

#define UNKNOWN			0
#define DUMPTOMEMORY	1
#define DUMPFROMMEMORY	2
#define READBLENFILE	3
#define WRITEBLENFILE	4
#define INFLATE			5
#define DEFLATE			6
#define DECRYPT			7
#define ENCRYPT			8
#define VERIFY			9
#define SIGN			10

#define MAXSTREAMLENGTH 10

#define STREAMGLUEHEADERSIZE sizeof(struct streamGlueHeaderStruct)

struct streamGlueHeaderStruct {
	uint8_t magic;					/* poor mans header recognize check */
	uint32_t totalStreamLength;		/* how much data is there */
	uint32_t dataProcessorType;		/* next data processing action */
	uint32_t crc;					/* header minus crc itself checksum */
};

struct readStreamGlueStruct {
	/* my control structure elements */
	unsigned int totalStreamLength;
	unsigned int streamDone;
	int dataProcessorType;
	void *ProcessorTypeControlStruct;

	unsigned char headerbuffer[STREAMGLUEHEADERSIZE];

	void *(*begin)(void *);
	int (*process)(void *, unsigned char *, unsigned int);
	int (*end)(void *);
};

        unsigned int
correctByteOrder(
        unsigned int x);

	int
readStreamGlue(
	void *endControl,
	struct readStreamGlueStruct **control,
	unsigned char *data,
	unsigned int dataIn);

#ifdef __cplusplus
}
#endif

#endif /* BLO_READSTREAMGLUE_H */

