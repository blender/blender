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
 */

/*
 * Unit test for the deflater
 *
 * The deflater compresses data, using the zlib compression
 * library. The BLO_deflate module wraps this. It writes the
 * compressed data as well.
 *
 * Tested functions
 *
 * - BLO_deflate (from BLO_deflate.h)
 *
 * Commandline arguments: <verbosity>
 * verbosity: 0 - print nothing
 *            1 - print the results only
 *            2 - print everything
 * 
 * */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "BLO_deflate.h"
#include "BLO_writeStreamGlue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct streamGlueControlStruct *Global_streamGlueControl;

void printStreamGlueHeader(struct streamGlueHeaderStruct *s)
{
	int i = 0;
	char* c;
	
	fprintf(stderr,"|   |- streamGlueHeader: %p\n",
			s);
	fprintf(stderr,"|      |- magic:             %c\n",
			s->magic);
	fprintf(stderr,"|      |- totalStreamLength: %x hex (%x reversed),"
			" %d dec (%d reversed)\n",
			s->totalStreamLength,
			ntohl(s->totalStreamLength),
			s->totalStreamLength,
			ntohl(s->totalStreamLength));
	fprintf(stderr,"|      |- dataProcessorType: %d (%d reversed)\n",
			s->dataProcessorType,
			ntohl(s->dataProcessorType));
	fprintf(stderr,"|      |- crc:               %x hex, (%d dec)\n",
			s->crc,
			s->crc);
	fprintf(stderr,"|\n");
	fprintf(stderr,"|-- Memory dump (starting at %p):", s);
	c = (char*) s;
	for(i = 0; i < STREAMGLUEHEADERSIZE; i+=4, c+=4)
		fprintf(stderr,"%02x%02x%02x%02x-",
				*c,
				*(c+1) ,
				*(c+2),
				*(c+3) );
	fprintf(stderr,"\n|\n");
	
}



int main (int argc, char *argv[])
{
	int verbose       = 0;
	int error_status  = 0;
	int retval        = 0;
	struct streamGlueHeaderStruct *streamGlueHeader;
	int datachunksize = 12345;
	char* datachunk   = NULL;
	int i             = 0;
	char* dataptr     = NULL;
	int sghsize       = 0;
	
   	switch (argc) {
	case 2:		
		verbose = atoi(argv[1]);
		if (verbose < 0) verbose = 0;
		break;		
	case 1:
	default:
		verbose = 0;
	}

	/* ----------------------------------------------------------------- */
	if (verbose > 0) {
		fprintf(stderr,"\n*** Deflate test with stubs\n|\n");
	}
	/* ----------------------------------------------------------------- */
	/* We need:
	 * 1 - a data chunk
	 * 2 - the bytecount
	 * 3 - a streamglueheader
	 * The streamglueheader contains some stats about the datachunk.
	 */

	/* because of stupid lib dependencies. */
	Global_streamGlueControl = streamGlueControlConstructor();

	
	/* 2: the size */
	datachunksize = 12345;

	/* 1: a data chunk. We fill it with some numbers */
	datachunk = (char*) malloc(datachunksize);

	/* an ascending-ish thingy */
	dataptr = datachunk;
	for (i = 0 ;
		   i < datachunksize;
		   i++, dataptr++) {
		*dataptr = (i % 0xFF);
	}
	
	/* 3: the streamglue header */
	sghsize = STREAMGLUEHEADERSIZE;
	if (verbose > 1) {
		fprintf(stderr,"|-- Allocating %d bytes for the header.\n",
				sghsize);
	}	
	streamGlueHeader = malloc(sghsize);
	streamGlueHeader->magic = 'A';
	streamGlueHeader->totalStreamLength = 0;
	streamGlueHeader->dataProcessorType =
		htonl(0x2);
	streamGlueHeader->crc = 0;

	
	if (verbose > 1) {
		fprintf(stderr,"|\n");
		fprintf(stderr,"|-- Will call BLO_deflate with args:\n");
		fprintf(stderr,"|   |- datachunk pointing to %p\n",
				datachunk);
		fprintf(stderr,"|   |- datachunksize: %d\n",
				datachunksize);
		printStreamGlueHeader(streamGlueHeader);
		fprintf(stderr,"| \n");
	}
	
	retval = 
		BLO_deflate(
			datachunk,
			datachunksize,
			streamGlueHeader);

	if (verbose > 1) {
		fprintf(stderr,"|-- BLO_deflate returned %d \n", retval);
	}	
	
	if (verbose > 1) {
		fprintf(stderr,"|\n");
		fprintf(stderr,"|-- Arguments are now:\n");
		fprintf(stderr,"|-- Will call BLO_deflate with args:\n");
		fprintf(stderr,"|   |- datachunk pointing to %p\n",
				datachunk);
		fprintf(stderr,"|   |- datachunksize: %d\n",
				datachunksize);
		printStreamGlueHeader(streamGlueHeader);
		fprintf(stderr,"| \n");
	}

	
	/* ----------------------------------------------------------------- */	
	if (verbose > 0) {
		fprintf(stderr,"|\n*** Finished test\n\n");
	}
	exit(error_status);
}

/* eof */
