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
 * zlib inflate decompression wrapper library
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "zlib.h"

#include "GEN_messaging.h"

#include "BLO_readStreamGlue.h"
#include "BLO_in_de_flateHeader.h"	/* used by deflate and inflate */

#include "BLO_inflate.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// TODO use other error function
static int CHECK_ERR(int err, char *msg);

static int CHECK_ERR(int err, char *msg)
{ 
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

struct inflateStructType {
	uInt compresSize;	/* fixed compresBuf size in bytes */
	Bytef *compresBuf;	/* reusable fixed size output buffer for inflate */
	struct readStreamGlueStruct *streamGlue;
	struct BLO_in_de_flateHeaderStruct *streamHeader;
	unsigned int streamDone;
	unsigned char headerbuffer[IN_DE_FLATEHEADERSTRUCTSIZE];
	z_stream d_stream;	/* decompression stream */
	char dictionary[50];
	uLong dictId;		/* Adler32 value of the dictionary */
	void *endControl;
};

/**
 * zlib inflate decompression initializer
 * @retval pointer to inflate control structure
 */
	BLO_inflateStructHandle
BLO_inflate_begin(
	void *endControl)
{
	int err = 0;     /* our own error */
	char *errmessage = "inflateInit";
	
	struct inflateStructType *control;
	control = malloc(sizeof(struct inflateStructType));
	if (!control) return NULL;

	control->compresSize = (100000 * 1.1) + 12;
	control->compresBuf = (Bytef *)malloc(control->compresSize);
	if (!control->compresBuf) {
		free(control);
		return NULL;
	}

	control->streamGlue = NULL;
	control->streamHeader = malloc(IN_DE_FLATEHEADERSTRUCTSIZE);
	if (!control->streamHeader) {
		free(control->compresBuf);
		free(control);
		return NULL; 
	}

	control->streamHeader->magic = 0;
	control->streamHeader->compressedLength = 0;
	control->streamHeader->uncompressedLength = 0;
	control->streamHeader->dictionary_id = 0;
	control->streamHeader->dictId = 0;
	control->streamHeader->crc = 0;
	control->streamDone = 0;
	memset(control->headerbuffer, 0, IN_DE_FLATEHEADERSTRUCTSIZE);
	control->d_stream.zalloc = (alloc_func)0;
	control->d_stream.zfree = (free_func)0;
	control->d_stream.opaque = (voidpf)0;
	// TODO use dictionary index, this is id = 1 :
	strcpy(control->dictionary, "sure this is not a number");

	/* we need to rewire this to also return err */
	err = inflateInit(&(control->d_stream));
	err = CHECK_ERR(err, errmessage);
	if (err) {
		free(control->compresBuf);
		free(control->streamHeader);
		free(control);
		return NULL;
	}

	control->dictId = control->d_stream.adler;

	control->d_stream.next_out = control->compresBuf;
	control->d_stream.avail_out = control->compresSize;

	control->d_stream.next_in = NULL;
	control->d_stream.avail_in = 0;

	control->endControl = endControl;
	return((BLO_inflateStructHandle) control);
}

/**
 * zlib inflate dataprocessor wrapper
 * @param BLO_inflate Pointer to inflate control structure
 * @param data Pointer to new data
 * @param dataIn New data amount
 * @retval streamGlueRead return value
 */
	int
BLO_inflate_process(
	BLO_inflateStructHandle BLO_inflate_handle,
	unsigned char *data,
	unsigned int dataIn)
{
	int zlib_err = 0;
	int err = 0;
	char *errmsg1 = "inflateSetDictionary";
	
	struct inflateStructType *BLO_inflate =
		(struct inflateStructType *) BLO_inflate_handle;

	if (!BLO_inflate) {
		err = BRS_SETFUNCTION(BRS_INFLATE) |
			  BRS_SETGENERR(BRS_NULL);
		return err;
	}

	/* First check if we have our header filled in yet */
	if (BLO_inflate->streamHeader->compressedLength == 0) {
		unsigned int processed;
		if (dataIn == 0) return err;	/* really need data to do anything */
		processed = ((dataIn + BLO_inflate->streamDone) <=
					 IN_DE_FLATEHEADERSTRUCTSIZE)
					 ? dataIn : IN_DE_FLATEHEADERSTRUCTSIZE;
		memcpy(BLO_inflate->headerbuffer + BLO_inflate->streamDone,
			   data, processed);
		BLO_inflate->streamDone += processed;
		dataIn -= processed;
		data += processed;
		if (BLO_inflate->streamDone == IN_DE_FLATEHEADERSTRUCTSIZE) {
			/* we have the whole header, absorb it */
			struct BLO_in_de_flateHeaderStruct *header;
			uint32_t crc;
			header = (struct BLO_in_de_flateHeaderStruct *)
				BLO_inflate->headerbuffer;
			BLO_inflate->streamHeader->compressedLength =
				ntohl(header->compressedLength);
			BLO_inflate->streamHeader->uncompressedLength =
				ntohl(header->uncompressedLength);
			BLO_inflate->streamHeader->dictId =
				ntohl(header->dictId);
			BLO_inflate->streamHeader->dictionary_id =
				ntohl(header->dictionary_id);
			crc = crc32(0L, (const Bytef *) header,
						IN_DE_FLATEHEADERSTRUCTSIZE - 4);

			if (header->magic == 'B') {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"BLO_in_de_flateHeaderStruct Magic confirmed\n");
#endif
			} else {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"ERROR BLO_in_de_flateHeaderStruct Magic NOT confirmed\n");
#endif
				err = BRS_SETFUNCTION(BRS_INFLATE) |
					  BRS_SETGENERR(BRS_MAGIC);
				if (BLO_inflate->streamGlue) free(BLO_inflate->streamGlue);
				if (BLO_inflate->streamHeader) free(BLO_inflate->streamHeader);
				if (BLO_inflate->compresBuf) free(BLO_inflate->compresBuf);
				free(BLO_inflate);
				return err;
			}

			if (crc == ntohl(header->crc)) {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"BLO_in_de_flateHeader CRC correct\n");
#endif
			} else {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"ERROR BLO_in_de_flateHeader CRC NOT correct\n");
#endif
				err = BRS_SETFUNCTION(BRS_INFLATE) |
					  BRS_SETGENERR(BRS_CRCHEADER);
				if (BLO_inflate->streamGlue) free(BLO_inflate->streamGlue);
				if (BLO_inflate->streamHeader) free(BLO_inflate->streamHeader);
				if (BLO_inflate->compresBuf) free(BLO_inflate->compresBuf);
				free(BLO_inflate);
				return err;
			}
			
#ifndef NDEBUG
			fprintf(GEN_errorstream,
					"BLO_inflate_process gets %u compressed bytes, will be %u uncompressed\n",
					(unsigned int) BLO_inflate->streamHeader->compressedLength,
					(unsigned int) BLO_inflate->streamHeader->uncompressedLength);
#endif

		}
	}

	/* Is there really (still) new data available ? */
	if (dataIn > 0) {
		int inflateWantsToLoopAgain = 0;
		BLO_inflate->d_stream.next_in = data;
		BLO_inflate->d_stream.avail_in = dataIn;
		do {
			zlib_err = inflate(&(BLO_inflate->d_stream), Z_SYNC_FLUSH);
			if (zlib_err == Z_NEED_DICT) {
				// TODO we can use BLO_inflate->d_stream.adler (it has
				// multiple uses) to select the dictionary to use. This is id=1
				zlib_err = inflateSetDictionary(&(BLO_inflate->d_stream),
								   (const Bytef*)BLO_inflate->dictionary,
								   strlen(BLO_inflate->dictionary));
				err = CHECK_ERR(zlib_err, errmsg1);
				if (err) {
					err = BRS_SETFUNCTION(BRS_INFLATE) |
						  BRS_SETSPECERR(BRS_INFLATEERROR);
					if (BLO_inflate->streamGlue) free(BLO_inflate->streamGlue);
					if (BLO_inflate->streamHeader) free(BLO_inflate->streamHeader);
					if (BLO_inflate->compresBuf) free(BLO_inflate->compresBuf);
					free(BLO_inflate);
					return err;
				}

				// go again
				zlib_err = inflate(&(BLO_inflate->d_stream), Z_SYNC_FLUSH);
			}
			if (zlib_err == Z_STREAM_END) {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"Note: inflate returned Z_STREAM_END\n");
#endif
			} else if (zlib_err != Z_OK) {
#ifndef NDEBUG
				fprintf(GEN_errorstream, "Error: inflate should return Z_OK, not %d\n", zlib_err);
#endif
				err = BRS_SETFUNCTION(BRS_INFLATE) |
					  BRS_SETSPECERR(BRS_INFLATEERROR);
				if (BLO_inflate->streamGlue) free(BLO_inflate->streamGlue);
				if (BLO_inflate->streamHeader) free(BLO_inflate->streamHeader);
				if (BLO_inflate->compresBuf) free(BLO_inflate->compresBuf);
				free(BLO_inflate);
				return err;
			}
			if (BLO_inflate->d_stream.avail_out == 0) {
#ifndef NDEBUG
				fprintf(GEN_errorstream,
						"Note: inflate needs more output space, loop again %u\n",
						(unsigned int) BLO_inflate->d_stream.avail_in);
#endif
				inflateWantsToLoopAgain = 1;
			} else {
#ifndef NDEBUG
				if (inflateWantsToLoopAgain == 1)
					fprintf(GEN_errorstream,
							"Note: this is inflates last loop\n");
#endif
				inflateWantsToLoopAgain = 0;

#ifndef NDEBUG
				fprintf(GEN_errorstream, "inflated %u to %u (flushes) err=%d\n",
						dataIn,
						(unsigned int) (BLO_inflate->compresSize - BLO_inflate->d_stream.avail_out),
						err);
#endif
			}

			// give data to streamGlueRead, it will find out what to do next
			err = readStreamGlue(
				BLO_inflate->endControl,
				&(BLO_inflate->streamGlue),
				BLO_inflate->compresBuf,
				BLO_inflate->compresSize - BLO_inflate->d_stream.avail_out);
			BLO_inflate->d_stream.next_out = BLO_inflate->compresBuf;
			BLO_inflate->d_stream.avail_out = BLO_inflate->compresSize;
		} while (inflateWantsToLoopAgain == 1);
	}
	return err;
}

/**
 * zlib inflate final call and cleanup
 * @param BLO_inflate Pointer to inflate control structure
 * @retval streamGlueRead return value
 */
	int
BLO_inflate_end(
	BLO_inflateStructHandle BLO_inflate_handle)
{
	char *errmsg2 = "inflateEnd";
	int err = 0;
	int zlib_err = 0;
	struct inflateStructType *BLO_inflate =
		(struct inflateStructType *) BLO_inflate_handle;
	// TODO perhaps check streamHeader->totalStreamLength

	if (!BLO_inflate) {
		err = BRS_SETFUNCTION(BRS_INFLATE) |
			  BRS_SETGENERR(BRS_NULL);
		return err;
	}
	
	BLO_inflate->d_stream.avail_in = 0;
	// Note: do not also set BLO_inflate->d_stream.next_in to NULL, it
	// is illegal (zlib.h:374) and causes a Z_STREAM_ERROR

	zlib_err = inflate(&(BLO_inflate->d_stream), Z_FINISH);
	if (zlib_err != Z_STREAM_END) {
#ifdef NDEBUG
		fprintf(GEN_errorstream,
				"inflate should report Z_STREAM_END, not %d\n",
				err);
		
		if (BLO_inflate->d_stream.avail_out == 0) {
			fprintf(GEN_errorstream,
					"Error: inflate wanted more output buffer space\n");
			// Note that we CANNOT inflate-loop again !
			// But this should never happen because we Z_SYNC_FLUSH
		}
#endif
		err = BRS_SETFUNCTION(BRS_INFLATE) |
			  BRS_SETSPECERR(BRS_INFLATEERROR);
		if (BLO_inflate->streamGlue) free(BLO_inflate->streamGlue);
		if (BLO_inflate->streamHeader) free(BLO_inflate->streamHeader);
		if (BLO_inflate->compresBuf) free(BLO_inflate->compresBuf);
		free(BLO_inflate);
		return err;
	}

	zlib_err = inflateEnd(&(BLO_inflate->d_stream));
	err = CHECK_ERR(zlib_err, errmsg2);
	if (err) {
		err = BRS_SETFUNCTION(BRS_INFLATE) |
			  BRS_SETSPECERR(BRS_INFLATEERROR);
		if (BLO_inflate->streamGlue) free(BLO_inflate->streamGlue);
		if (BLO_inflate->streamHeader) free(BLO_inflate->streamHeader);
		if (BLO_inflate->compresBuf) free(BLO_inflate->compresBuf);
		free(BLO_inflate);
		return err;
	}

	if (BLO_inflate->d_stream.adler != BLO_inflate->dictId) {
		// data was corrupted
#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"Failed adler checksum\n");
#endif 
		err = BRS_SETFUNCTION(BRS_INFLATE) |
			  BRS_SETGENERR(BRS_CRCDATA);
		if (BLO_inflate->streamGlue) free(BLO_inflate->streamGlue);
		if (BLO_inflate->streamHeader) free(BLO_inflate->streamHeader);
		if (BLO_inflate->compresBuf) free(BLO_inflate->compresBuf);
		free(BLO_inflate);
		return err;
	} else {
#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"Passed adler checksum\n");
#endif
	}

	/* ready decompressing */
#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"DeCompressed %ld bytes to %ld (%.0f%%)\n",
			BLO_inflate->d_stream.total_in, BLO_inflate->d_stream.total_out,
			100. * (float)BLO_inflate->d_stream.total_out /
			(float)BLO_inflate->d_stream.total_in);
#endif

	err = readStreamGlue(
		BLO_inflate->endControl,
		&(BLO_inflate->streamGlue),
		BLO_inflate->compresBuf,
		BLO_inflate->compresSize - BLO_inflate->d_stream.avail_out);
	
	BLO_inflate->d_stream.next_out = BLO_inflate->compresBuf;
	BLO_inflate->d_stream.avail_out = BLO_inflate->compresSize;
	
	if (BLO_inflate->streamGlue) free(BLO_inflate->streamGlue);
	if (BLO_inflate->streamHeader) free(BLO_inflate->streamHeader);
	if (BLO_inflate->compresBuf) free(BLO_inflate->compresBuf);
	free(BLO_inflate);
	
	return err;
}

