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
 * zlib deflate compression wrapper library
 */

#include <stdio.h>
#include <stdlib.h>

#include "zlib.h"

#include "GEN_messaging.h"

#include "BLO_writeStreamGlue.h"
#include "BLO_deflate.h"
#include "BLO_in_de_flateHeader.h"

// TODO use other error function
static int CHECK_ERR(int err, char *msg);

static int CHECK_ERR(int err, char *msg) { 
	if (err != Z_OK) {
#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"%s error: %d\n",
				msg,
				err); 
#endif
		return 1;
	}
	return 0;
}

	int
BLO_deflate(
	unsigned char *data,
	unsigned int dataIn,
	struct streamGlueHeaderStruct *streamGlueHeader)
{
	int zlib_err;		/* zlib error */
	int err = 0;        /* our own error */
	z_stream c_stream;	/* compression stream */
	char dictionary[50];
	Bytef *compressBuf;	/* minimally sized output buffer for deflate */
	uInt compressSize;	/* minimally sized compressBuf size in bytes */
	struct writeStreamGlueStruct *streamGlue = NULL;
	struct BLO_in_de_flateHeaderStruct BLO_in_de_flateHeader;
	char* errmsg1 = "deflateInit";
	char* errmsg2 = "deflateSetDictionary";
	char* errmsg3 = "deflateEnd";

	// TODO use dictionary index, this is id = 1 :
	strcpy(dictionary, "sure this is not a number");

	compressSize = (dataIn * 1.1) + 12;
	compressBuf = (Bytef *)malloc(compressSize);
	if (!compressBuf) {
		err = BWS_SETFUNCTION(BWS_DEFLATE) |
			  BWS_SETGENERR(BWS_MALLOC);
		return err;
	}

	c_stream.next_out = compressBuf;
	c_stream.avail_out = compressSize;
	c_stream.next_in = data;
	c_stream.avail_in = dataIn;

	c_stream.zalloc = (alloc_func)0;
	c_stream.zfree = (free_func)0;
	c_stream.opaque = (voidpf)0;

	zlib_err = deflateInit(&c_stream, Z_BEST_COMPRESSION);
	if (CHECK_ERR(zlib_err, errmsg1)) {
		err = BWS_SETFUNCTION(BWS_DEFLATE) |
			  BWS_SETSPECERR(BWS_DEFLATEERROR);
		free(compressBuf);
		return err;
	}

	zlib_err = deflateSetDictionary(&c_stream,
									(const Bytef*)dictionary,
									strlen(dictionary));
	if (CHECK_ERR(zlib_err, errmsg2)) {
		err = BWS_SETFUNCTION(BWS_DEFLATE) |
			  BWS_SETSPECERR(BWS_DEFLATEERROR);
		free(compressBuf);
		return err;
	}
	
	// Compress it
	zlib_err = deflate(&c_stream, Z_FINISH);
	if (zlib_err != Z_STREAM_END) {
#ifndef NDEBUG		
		fprintf(GEN_errorstream,
				"deflate should report Z_STREAM_END\n");
#endif
		// (avail_out == 0) possibility ? Should not, because we
		// malloc by the minimal needed amount rule
		err = BWS_SETFUNCTION(BWS_DEFLATE) |
			  BWS_SETSPECERR(BWS_DEFLATEERROR);
		free(compressBuf);
		return err;
	}

	zlib_err = deflateEnd(&c_stream);
	if (CHECK_ERR(zlib_err, errmsg3)) {
		err = BWS_SETFUNCTION(BWS_DEFLATE) |
			  BWS_SETSPECERR(BWS_DEFLATEERROR);
		free(compressBuf);
		return err;
	}

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_deflate compressed %ld bytes to %ld (%.0f%%)\n",			
			c_stream.total_in, c_stream.total_out,
			100. * (float)c_stream.total_out / (float)c_stream.total_in);
	
	fprintf(GEN_errorstream,
			"BLO_deflate writes streamGlueHeader of %u bytes\n",
			STREAMGLUEHEADERSIZE);
#endif
	// Update streamGlueHeader that initiated us and write it away
	streamGlueHeader->totalStreamLength =
		htonl(IN_DE_FLATEHEADERSTRUCTSIZE + c_stream.total_out);
	streamGlueHeader->crc = htonl(crc32(0L, (const Bytef *) streamGlueHeader,
					STREAMGLUEHEADERSIZE - 4));
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *) streamGlueHeader,
		STREAMGLUEHEADERSIZE,
		0);
	if (err) {
		free(compressBuf);
		return err;
	}

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_deflate writes BLO_in_de_flateHeader of %u bytes\n",
			IN_DE_FLATEHEADERSTRUCTSIZE);
#endif

	// write out our header
	BLO_in_de_flateHeader.magic = 'B';
	BLO_in_de_flateHeader.compressedLength = htonl(c_stream.total_out);
	BLO_in_de_flateHeader.uncompressedLength = htonl(c_stream.total_in);
	BLO_in_de_flateHeader.dictionary_id = htonl(1);
	BLO_in_de_flateHeader.dictId = htonl(c_stream.adler); // adler checksum
	BLO_in_de_flateHeader.crc = htonl(crc32(0L,
		(const Bytef *) &BLO_in_de_flateHeader, IN_DE_FLATEHEADERSTRUCTSIZE-4));
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *) &BLO_in_de_flateHeader,
		IN_DE_FLATEHEADERSTRUCTSIZE,
		0);
	if (err) {
		free(compressBuf);
		return err;
	}

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_deflate writes %lu bytes raw data (total %lu)\n",
			c_stream.total_out, STREAMGLUEHEADERSIZE +
			IN_DE_FLATEHEADERSTRUCTSIZE + c_stream.total_out);
#endif

	// finally write all compressed data
	err = writeStreamGlue(
		Global_streamGlueControl,
		&streamGlue,
		(unsigned char *) compressBuf,
		c_stream.total_out,
		1);

	free(compressBuf);

	return err;
}

