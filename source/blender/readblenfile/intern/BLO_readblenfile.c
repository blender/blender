/*
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
/**
 * \file BLO_readblenfile.c
 * \brief This file handles the loading if .blend files
 * \ingroup mainmodule
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <io.h>		// read, open
#else // ! WIN32
#include <unistd.h>		// read
#endif

#include "BLO_readStreamGlue.h"

#include "BLO_readfile.h"
#include "BLO_readblenfile.h"

#include "BKE_blender.h"

#define CACHESIZE 100000

/** Magic number for the file header */
char *headerMagic = "BLENDFI";

/**
 * \brief Set the version number into the array.
 *
 * version contains the integer number of the version
 * i.e. 227
 * array[1] gets set to the div of the number by 100 i.e. 2
 * array[2] gets the remainder i.e. 27
 */
void BLO_setversionnumber(char array[4], int version)
{
	memset(array, 0, sizeof(array));

	array[1] = version / 100;
	array[2] = version % 100;
}

/**
 * Sets version number using BLENDER_VERSION
 * Function that calls the setversionnumber(char[],int) with 
 * the BLENDER_VERSION constant and sets the resultant array
 * with the version parts.  
 * see BLO_setversionnumber(char[],int).
 */
void BLO_setcurrentversionnumber(char array[4])
{
	BLO_setversionnumber(array, BLENDER_VERSION);
}

#ifndef O_BINARY
#define O_BINARY 0
#endif

/**
 * Defines the data struct for the .blend file
 */
struct BLO_readblenfileStruct {
	struct readStreamGlueStruct *streamGlue;
	int fileDes;
	unsigned int cacheSize;
	unsigned int inCache;
	unsigned int leftToRead;
	unsigned int Seek;

	int (*read)(struct BLO_readblenfileStruct *readblenfileStruct, void *buffer, int size);

	char *readCache;
	char *fromBuffer;
	int  fromBufferSize;
	char crInBuffer;
	char removeCR;
};

// declare static functions

static int readfromfilehandle(
	struct BLO_readblenfileStruct *readblenfileStruct,
	void *buffer,
	int size);

static int readfrommemory(
	struct BLO_readblenfileStruct *readblenfileStruct,
	void *buffer,
	int size);

static int fillcache(
	struct BLO_readblenfileStruct *readblenfileStruct);

static unsigned int readfromcache(
	struct BLO_readblenfileStruct *readblenfileStruct,
	void * buffer,
	unsigned int size);

static BlendFileData *readblenfilegeneric(
	struct BLO_readblenfileStruct *readblenfileStruct, 
	BlendReadError *error_r);

// implementation of static functions

/**
 * \brief Reads data from the already opened file.
 * Given the file structure a buffer and the size of the block
 * the function will read from the file if it is open.
 * If not it will return -1 indicating an unopened file.
 * 
 * \return Returns the size of the file read, or -1.
 */
static int readfromfilehandle(
	struct BLO_readblenfileStruct *readblenfileStruct,
	void *buffer,
	int size)
{
	int readsize = -1;

	if (readblenfileStruct->fileDes != -1) {
		readsize = read(readblenfileStruct->fileDes, buffer, size);
	}

	return(readsize);
}

/**
 * \brief Reads and erases from readblenfileStruct->fromBuffer
 *
 * Copies information from the from the fromBuffer to the buffer, then 
 * decrements the size of the fromBuffer, and moves the pointer along
 * thereby effectively removing the data forever.
 *
 * \return Returns the size of the read from memory
 */	
static int readfrommemory(
	struct BLO_readblenfileStruct *readblenfileStruct,
	void *buffer,
	int size)
{
	int readsize = -1;

	if (readblenfileStruct->fromBuffer) {
		if (size > readblenfileStruct->fromBufferSize) {
			size = readblenfileStruct->fromBufferSize;
		}

		memcpy(buffer, readblenfileStruct->fromBuffer, size);
		readblenfileStruct->fromBufferSize -= size;
		readblenfileStruct->fromBuffer += size;

		readsize = size;
	}

	return(readsize);
}

/**
 * Read in data from the file into a cache.
 *
 * \return Returns the size of the read.
 *
 * \attention Note: there is some code missing to return CR if the 
 * structure indicates it.
*/
static int fillcache(
	struct BLO_readblenfileStruct *readblenfileStruct)
{
	int readsize;
	int toread;

	// how many bytes can we read ?

	toread = readblenfileStruct->leftToRead;

	if (toread > readblenfileStruct->cacheSize) {
		toread = readblenfileStruct->cacheSize;
	}

	readsize = readblenfileStruct->read(readblenfileStruct, readblenfileStruct->readCache, toread);
	if (readsize > 0) {
		if (readblenfileStruct->removeCR) {
			// do some stuff here
		}
		readblenfileStruct->inCache = readsize;
		readblenfileStruct->leftToRead -= readsize;
	}

	return (readsize);
}

/**
 * \brief Read data from the cache into a buffer.
 * Marks the last read location with a seek value.
 *
 * \return Returns the size of the read from the cache.
 *
 * \attention Note: missing some handling code if the location is
 * \attention outside of the cache.
 */
static unsigned int readfromcache(
	struct BLO_readblenfileStruct *readblenfileStruct,
	void * buffer,
	unsigned int size)
{
	unsigned int readsize = 0;

	if (readblenfileStruct->inCache - readblenfileStruct->Seek > size) {
		memcpy(buffer, readblenfileStruct->readCache + readblenfileStruct->Seek, size);
		readblenfileStruct->Seek += size;
		readsize = size;
	} else {
		// handle me
	}

	return(readsize);
}

/**
 * \brief Converts from BRS error code to BRE error code.
 *
 * Error conversion method to convert from
 * the BRS type errors and return a BRE 
 * type error code.
 * Decodes based on the function, the generic,
 * and the specific portions of the error.
 */
static BlendReadError brs_to_bre(int err)
{
	int errFunction = BRS_GETFUNCTION(err);
	int errGeneric =  BRS_GETGENERR(err);
	int errSpecific = BRS_GETSPECERR(err);

	if (errGeneric) {
		switch (errGeneric) {
		case BRS_MALLOC:
			return BRE_OUT_OF_MEMORY;
		case BRS_NULL:
			return BRE_INTERNAL_ERROR;
		case BRS_MAGIC:
			return BRE_NOT_A_BLEND;
		case BRS_CRCHEADER:
		case BRS_CRCDATA:
			return BRE_CORRUPT;
		case BRS_DATALEN:
			return BRE_INCOMPLETE;
		case BRS_STUB:
			return BRE_NOT_A_BLEND;
		}
	} else if (errSpecific) {
		switch (errFunction) {
		case BRS_READSTREAMGLUE:
			switch (errSpecific) {
			case BRS_UNKNOWN:
				return BRE_INTERNAL_ERROR;
			}
			break;
		case BRS_READSTREAMFILE:
			switch (errSpecific) {
			case BRS_NOTABLEND:
				return BRE_NOT_A_BLEND;
			case BRS_READERROR:
				return BRE_UNABLE_TO_READ;
			}
			break;
		case BRS_INFLATE:
			switch (errSpecific) {
			case BRS_INFLATEERROR:
				return BRE_CORRUPT;
			}
			break;
		case BRS_DECRYPT:
			switch (errSpecific) {
			case BRS_RSANEWERROR:
				return BRE_INTERNAL_ERROR;
			case BRS_DECRYPTERROR:
				return BRE_INTERNAL_ERROR;
			case BRS_NOTOURPUBKEY:
				return BRE_NOT_ALLOWED;
			}
			break;
		case BRS_VERIFY:
			switch (errSpecific) {
			case BRS_RSANEWERROR:
				return BRE_INTERNAL_ERROR;
			case BRS_SIGFAILED:
				return BRE_INTERNAL_ERROR;
			}
			break;
		}
	}
	
	return BRE_INVALID;
}

static BlendFileData *readblenfilegeneric(
	struct BLO_readblenfileStruct *readblenfileStruct, 
	BlendReadError *error_r)
{
	BlendFileData *bfd= NULL;
	unsigned char reserved[BLO_RESERVEDSIZE];
	uint8_t minversion[4];
	uint8_t myversion[4];
	uint8_t version[4];
	uint8_t flags[4];
	void *parms[2];
	int filesize;
	
	parms[0]= &bfd;
	parms[1]= error_r;

	BLO_setcurrentversionnumber(myversion);

	readblenfileStruct->cacheSize      = CACHESIZE;
	readblenfileStruct->readCache      = malloc(readblenfileStruct->cacheSize);

	if (fillcache(readblenfileStruct) <= 0) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (readfromcache(readblenfileStruct, minversion, sizeof(minversion)) != sizeof(minversion)) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (memcmp(minversion, myversion, sizeof(minversion)) > 0) {
		*error_r = BRE_TOO_NEW;
	} else if (readfromcache(readblenfileStruct, version,  sizeof(version)) != sizeof(version)) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (readfromcache(readblenfileStruct, flags,    sizeof(flags)) != sizeof(flags)) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (readfromcache(readblenfileStruct, &filesize, sizeof(filesize)) != sizeof(filesize)) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (readfromcache(readblenfileStruct, reserved, sizeof(reserved)) != sizeof(reserved)) {
		*error_r = BRE_UNABLE_TO_READ;
	}  else {
		filesize = ntohl(filesize);

		// substract number of bytes we've
		// been handling outside readfromcache()
		filesize -= strlen(headerMagic);
		filesize--;

		if (filesize < readblenfileStruct->inCache) {
			// we've allready read more than we're supposed to
			readblenfileStruct->inCache   = filesize;
			readblenfileStruct->leftToRead = 0;									
		} else {
			// 
			readblenfileStruct->leftToRead = filesize - readblenfileStruct->inCache;
		}

		do {
			int err;

			*error_r = BRE_NONE;
			err = readStreamGlue(
				parms,
				&(readblenfileStruct->streamGlue),
				readblenfileStruct->readCache + readblenfileStruct->Seek,
				readblenfileStruct->inCache - readblenfileStruct->Seek);

			readblenfileStruct->inCache = 0;
			readblenfileStruct->Seek = 0;

			if (err) {
				bfd = NULL;

					/* If *error_r != BRE_NONE then it is
					 * blo_readstreamfile_end signaling an error
					 * in the loading code. Otherwise it is some
					 * other part of the streamglue system signalling
					 * and error so we convert the BRS error into
					 * a BRE error.
					 * 
					 * Does this have to be so convoluted? No.
					 */
				if (*error_r == BRE_NONE) {
					*error_r = brs_to_bre(err);
				}
				
				break;
			}
		} while (fillcache(readblenfileStruct) > 0);
	}

	free(readblenfileStruct->readCache);
	readblenfileStruct->readCache = 0;

	return bfd;
}

// implementation of exported functions

BlendFileData *
BLO_readblenfilememory(
	char *fromBuffer, 
	int fromBufferSize, 
	BlendReadError *error_r)
{
	static char *functionality_check= "\0FUNCTIONALITY_CHECK += BLO_readblenfilememory\n";
	int magiclen = strlen(headerMagic);
	BlendFileData *bfd = NULL;

	if (!fromBuffer) {
		*error_r = BRE_UNABLE_TO_OPEN;
	} else if (fromBufferSize < magiclen) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (strncmp(fromBuffer, headerMagic, magiclen) != 0) {
		*error_r = BRE_NOT_A_BLEND;
	} else if (fromBufferSize < magiclen+1) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (fromBuffer[magiclen] != '\r' && fromBuffer[magiclen] != '\n') {
		*error_r = BRE_NOT_A_BLEND;
	} else {
		int crnl; 

		fromBuffer+= magiclen;
		fromBufferSize-= magiclen;
		crnl = (fromBuffer[0] == '\r');
		fromBuffer++;
		fromBufferSize--;
		
		if (crnl && fromBufferSize<1) {
			*error_r = BRE_UNABLE_TO_READ;
		} else {
			struct BLO_readblenfileStruct *readblenfileStruct = NULL;

				/* skip carriage return if necessary */
			if (crnl) {
				fromBuffer++;
				fromBufferSize--;
			}

			// Allocate all the stuff we need
			readblenfileStruct = calloc(sizeof(struct BLO_readblenfileStruct), 1);
			readblenfileStruct->fileDes        = -1;
			readblenfileStruct->fromBuffer     = fromBuffer;
			readblenfileStruct->fromBufferSize = fromBufferSize;
			readblenfileStruct->read		   = readfrommemory;

			readblenfileStruct->removeCR   = crnl;
			// fake filesize for now until we've
			// actually read in the filesize from the header
			// make sure we don't read more bytes than there
			// are left to handle accoding to fromBufferSize
			readblenfileStruct->leftToRead = readblenfileStruct->fromBufferSize;

			bfd = readblenfilegeneric(readblenfileStruct, error_r);

			free(readblenfileStruct);
			readblenfileStruct = 0;
		}
	}

	return bfd;
}


BlendFileData *
BLO_readblenfilehandle(
	int fd, 
	BlendReadError *error_r)
{
	static char *functionality_check= "\0FUNCTIONALITY_CHECK += BLO_readblenfilehandle\n";
	int magiclen = strlen(headerMagic);
	BlendFileData *bfd = NULL;
	char tempbuffer[256];
	
	if (fd==-1) {
		*error_r = BRE_UNABLE_TO_OPEN;
	} else if (read(fd, tempbuffer, magiclen) != magiclen) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (strncmp(tempbuffer, headerMagic, magiclen) != 0 ) {
		*error_r = BRE_NOT_A_BLEND;
	} else if (read(fd, tempbuffer, 1) != 1) {
		*error_r = BRE_UNABLE_TO_READ;
	} else if (tempbuffer[0] != '\r' && tempbuffer[0] != '\n') {
		*error_r = BRE_NOT_A_BLEND;
	} else {
		int crnl = (tempbuffer[0] == '\r');
		
		if (crnl && read(fd, tempbuffer, 1)!=1) {
			*error_r = BRE_UNABLE_TO_READ;
		} else {
			struct BLO_readblenfileStruct *readblenfileStruct;

			// Allocate all the stuff we need
			readblenfileStruct = calloc(sizeof(struct BLO_readblenfileStruct), 1);
			readblenfileStruct->fileDes    = fd;
			readblenfileStruct->read       = readfromfilehandle;

			readblenfileStruct->removeCR   = crnl;
			// fake filesize for now until we've
			// actually read in the filesize from the header
			readblenfileStruct->leftToRead = CACHESIZE;

			bfd = readblenfilegeneric(readblenfileStruct, error_r);

			free(readblenfileStruct);
			readblenfileStruct = 0;
		}
	}

	return bfd;
}

BlendFileData *
BLO_readblenfilename(
	char *fileName, 
	BlendReadError *error_r)
{
	static char *functionality_check= "\0FUNCTIONALITY_CHECK += BLO_readblenfilename\n";
	BlendFileData *bfd = NULL;
	int fd;

	fd = open(fileName, O_RDONLY | O_BINARY);
	if (fd==-1) {
		*error_r= BRE_UNABLE_TO_OPEN;
	} else {
		bfd = BLO_readblenfilehandle(fd, error_r);
	}

	if (fd!=-1)
		close(fd);

	return bfd;
}

	/* Runtime reading */

static int handle_read_msb_int(int handle) {
	unsigned char buf[4];

	if (read(handle, buf, 4)!=4)
		return -1;
	else
		return (buf[0]<<24) + (buf[1]<<16) + (buf[2]<<8) + (buf[3]<<0);
}

int blo_is_a_runtime(char *path) {
	int res= 0, fd= open(path, O_BINARY|O_RDONLY, 0);
	int datastart;
	char buf[8];

	if (fd==-1)
		goto cleanup;
	
	lseek(fd, -12, SEEK_END);
	
	datastart= handle_read_msb_int(fd);
	if (datastart==-1)
		goto cleanup;
	else if (read(fd, buf, 8)!=8)
		goto cleanup;
	else if (memcmp(buf, "BRUNTIME", 8)!=0)
		goto cleanup;
	else
		res= 1;

cleanup:
	if (fd!=-1)
		close(fd);

	return res;	
}

BlendFileData *
blo_read_runtime(
	char *path, 
	BlendReadError *error_r) 
{
	static char *functionality_check= "\0FUNCTIONALITY_CHECK += BLO_read_runtime\n";
	BlendFileData *bfd= NULL;
	int fd, datastart;
	char buf[8];

	fd= open(path, O_BINARY|O_RDONLY, 0);
	if (fd==-1) {
		*error_r= BRE_UNABLE_TO_OPEN;
		goto cleanup;
	}

	lseek(fd, -12, SEEK_END);

	datastart= handle_read_msb_int(fd);
	if (datastart==-1) {
		*error_r= BRE_UNABLE_TO_READ;
		goto cleanup;
	} else if (read(fd, buf, 8)!=8) {
		*error_r= BRE_UNABLE_TO_READ;
		goto cleanup;
	} else if (memcmp(buf, "BRUNTIME", 8)!=0) {
		*error_r= BRE_NOT_A_BLEND;
		goto cleanup;
	} else {	
		lseek(fd, datastart, SEEK_SET);
		bfd= BLO_readblenfilehandle(fd, error_r);
	}
	
cleanup:
	if (fd!=-1)
		close(fd);
	
	return bfd;
}

#if 0
static char *brs_error_to_string(int err) {
	int errFunction = BRS_GETFUNCTION(err);
	int errGeneric =  BRS_GETGENERR(err);
	int errSpecific = BRS_GETSPECERR(err);
	char *errFunctionStrings[] = {
		"",
		"The read stream",
		"The read stream loopback",
		"The key store",
		"The file reading",
		"Decompressing the file",
		"Decrypting the file",
		"Verifying the signature"};
	char *errGenericStrings[] = {
		"",
		"generated an out of memory error",
		"bumped on an internal programming error",
		"did not recognize this as a blend file",
		"failed a blend file check",
		"bumped on corrupted data",
		"needed the rest of the blend file",
		"is not allowed in this version"};
	char *errReadStreamGlueStrings[] = {
		"",
		"does not know how to proceed"};
	char *errReadStreamFileStrings[] = {
		"",
		"did not recognize this as a blend file",
		"was busted on a read error"};
	char *errInflateStrings[] = {
		"",
		"bumped on a decompress error"};
	char *errDecryptStrings[] = {
		"",
		"could not make a new key",
		"bumped on a decrypt error",
		"was not allowed. This blend file is not made by you."};
	char *errVerifyStrings[] = {
		"",
		"could not make a new key",
		"failed"};
	char *errFunctionString= errFunctionStrings[errFunction];
	char *errExtraString= "";
	char *errString;
	
	if (errGeneric) {
		errExtraString= errGenericStrings[errGeneric];
	} else if (errSpecific) {
		switch (errFunction) {
		case BRS_READSTREAMGLUE:
			errExtraString= errReadStreamGlueStrings[errSpecific];
			break;
		case BRS_READSTREAMFILE:
			errExtraString= errReadStreamFileStrings[errSpecific];
			break;
		case BRS_INFLATE:
			errExtraString= errInflateStrings[errSpecific];
			break;
		case BRS_DECRYPT:
			errExtraString= errDecryptStrings[errSpecific];
			break;
		case BRS_VERIFY:
			errExtraString= errVerifyStrings[errSpecific];
			break;
		default:
			break;
		}
	}
	
	errString= MEM_mallocN(strlen(errFunctionString) + 1 + strlen(errExtraString) + 1);
	sprintf(errString, "%s %s", errFunctionString, errExtraString);
	
	return errString;
}
#endif


