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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // strlen

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#include <io.h>        // open / write
#else // WIN32
#include <unistd.h>    // write
#endif // WIN32

#include <fcntl.h>     // open

#include "zlib.h"

#include "GEN_messaging.h"

#include "BLO_readfile.h"

#include "BLO_writeStreamGlue.h"
#include "BLO_writeblenfile.h"
#include "BLO_readblenfile.h"

// some systems don't have / support O_BINARY
// on those systems we define it to 0 

#ifndef O_BINARY
#define O_BINARY 0
#endif

struct BLO_writeblenfileStruct {
	struct writeStreamGlueStruct **streamGlue;
	int fileDes;
	unsigned int bufferSize;
	unsigned int inBuffer;
	char *writeBuffer;
	int filestartoffset;
	int filesizeoffset;
};

	int
BLO_writeblenfile_process(
	struct BLO_writeblenfileStruct *control,
	unsigned char *data,
	unsigned int dataIn);

/**
 * Flushes buffered data to file
 * @param control structure holding all variables
 * @return 0 on success, 1 if write failed
 */

static int flushbuffer(
	struct BLO_writeblenfileStruct *control)
{
	int err = 0;
	unsigned int written;

	if (control->inBuffer) {
		written = write(control->fileDes, control->writeBuffer, control->inBuffer);
		if (written != control->inBuffer) {
			err = BWS_SETFUNCTION(BWS_WRITEBLENFILE) |
				  BWS_SETSPECERR(BWS_WRITE);
		} else {
			control->inBuffer = 0;
		}
	}

	return (err);
}


int BLO_writeblenfile(
	unsigned char *data,
	unsigned int dataIn,
	struct streamGlueHeaderStruct *streamGlueHeader)
{
	static char *functionality_check= "\0FUNCTIONALITY_CHECK += BLO_writeblenfile\n";
	struct BLO_writeblenfileStruct *writeblenfileStruct = 0;
	int fileDes;	
	extern int mywfile;
	int err = 0;
	char minversion[4];
	char version[4];
	char flags[4];
	unsigned int filesize;
	char reserved[BLO_RESERVEDSIZE];

	fileDes = mywfile;

	if (fileDes == -1) {
		/* The filedescriptor was bad: this is an internal error */
		err = BWS_SETFUNCTION(BWS_WRITEBLENFILE) |
			  BWS_SETSPECERR(BWS_FILEDES);
		return err;
	}

	writeblenfileStruct = calloc(1, sizeof(struct BLO_writeblenfileStruct));
	if (!writeblenfileStruct) {
		err = BWS_SETFUNCTION(BWS_WRITEBLENFILE) |
			  BWS_SETGENERR(BWS_MALLOC);
		return err;
	}
		
	writeblenfileStruct->bufferSize  = 100000;
	writeblenfileStruct->writeBuffer = malloc(writeblenfileStruct->bufferSize);
	if (!writeblenfileStruct->writeBuffer) {
		err = BWS_SETFUNCTION(BWS_WRITEBLENFILE) |
			  BWS_SETGENERR(BWS_MALLOC);
		return err;
	}

	writeblenfileStruct->fileDes     = fileDes;
	writeblenfileStruct->filestartoffset = lseek(fileDes, 0, SEEK_CUR);

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_writeblenfile writes headerMagic ...\n");
#endif

	// write our own magic fileheader
	err = BLO_writeblenfile_process(writeblenfileStruct,
									headerMagic,
									strlen(headerMagic));
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}

	// write out the '\n' that we use for the 
	// cr / nl conversion
	err = BLO_writeblenfile_process(writeblenfileStruct,
									"\n",
									1);
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}

	BLO_setversionnumber(minversion, 221);
	err = BLO_writeblenfile_process(writeblenfileStruct,
									minversion,
									sizeof(minversion));
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}
		
	BLO_setcurrentversionnumber(version);
	err = BLO_writeblenfile_process(writeblenfileStruct,
									version,
									sizeof(version));
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}
		
	memset(flags, 0, sizeof(flags));
	err = BLO_writeblenfile_process(writeblenfileStruct,
									flags,
									sizeof(flags));
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}

	// we'll have to write out the filesize in the end
	// remember the location in the file but make sure to
	// flush all cached data first...

	flushbuffer(writeblenfileStruct);
	writeblenfileStruct->filesizeoffset = lseek(fileDes, 0, SEEK_CUR);
	memset(&filesize, 0, sizeof(filesize));
	err = BLO_writeblenfile_process(writeblenfileStruct,
									&filesize,
									sizeof(filesize));
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}

	memset(reserved, 0, sizeof(reserved));
	err = BLO_writeblenfile_process(writeblenfileStruct,
									reserved,
									sizeof(reserved));
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}
#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_writeblenfile writes streamGlueHeader of %u bytes\n",
			STREAMGLUEHEADERSIZE);
#endif

	// Update streamGlueHeader that initiated us and write it away
	// Note that streamGlueHeader is *behind* the magic fileheader
	streamGlueHeader->totalStreamLength = htonl(0 + dataIn);
	streamGlueHeader->crc = htonl(crc32(0L,
		(const Bytef *) streamGlueHeader, STREAMGLUEHEADERSIZE - 4));
	err = BLO_writeblenfile_process(writeblenfileStruct,
									(unsigned char *) streamGlueHeader,
									STREAMGLUEHEADERSIZE);
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_writeblenfile writes %u bytes raw data\n",
			dataIn);
#endif
		
	// write raw data
	err = BLO_writeblenfile_process(writeblenfileStruct,
									data,
									dataIn);
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}

	err = flushbuffer(writeblenfileStruct);
	if (err) {
		free(writeblenfileStruct->writeBuffer);
		free(writeblenfileStruct);
		return err;
	}

	// write filesize in header
	// calculate filesize
	filesize = lseek(fileDes, 0, SEEK_CUR);
	filesize -= writeblenfileStruct->filestartoffset;

#ifndef NDEBUG
	fprintf(GEN_errorstream,
			"BLO_writeblenfile total file size %u bytes\n",
			filesize);
#endif

	// TODO There should be error catching here as well.
	// goto filesize location in file
	lseek(fileDes, writeblenfileStruct->filesizeoffset, SEEK_SET);
	// and write out filesize in network byte order
	filesize = htonl(filesize);
	write(fileDes, &filesize, sizeof(filesize));
	// seek to end of file to cover up our tracks
	lseek(fileDes, 0, SEEK_END);

	// clean up
	free(writeblenfileStruct->writeBuffer);
	free(writeblenfileStruct);

	return (err);
}

/**
 * Buffers data and writes it to disk when necessary
 * @param control structure holding all variables
 * @param dataIn Length of new chunk of data
 * @param data Pointer to new chunk of data
 */
	int
BLO_writeblenfile_process(
	struct BLO_writeblenfileStruct *control,
	unsigned char *data,
	unsigned int dataIn)
{
	int err = 0;
	unsigned int written;

	if (control && data) {
		if (dataIn) {
			// do we need to flush data ?
			if ((dataIn + control->inBuffer) > control->bufferSize) {
				err = flushbuffer(control);
			}

			if (! err) {
				// do we now have enough space in the buffer ?
				if ((dataIn + control->inBuffer) <= control->bufferSize) {
					// yes, just copy it to the buffer
					memcpy(control->writeBuffer + control->inBuffer, data, dataIn);
					control->inBuffer += dataIn;
				} else {
					// write data out immediately
					written = write(control->fileDes, data, dataIn);
					if (written != dataIn) {
						err = BWS_SETFUNCTION(BWS_WRITEBLENFILE) |
							  BWS_SETSPECERR(BWS_WRITE);
					}
				}
			}
		}
	} else {
#ifndef NDEBUG
		fprintf(GEN_errorstream,
				"BLO_writeblenfile_process: invalid parameters\n");
#endif
		err = BWS_SETFUNCTION(BWS_WRITEBLENFILE) |
			  BWS_SETSPECERR(BWS_PARAM);
	}

	return err;
}

