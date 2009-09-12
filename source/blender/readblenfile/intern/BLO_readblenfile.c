/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
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
#include "BLI_winstuff.h"
#else // ! WIN32
#include <unistd.h>		// read
#endif

#include "BLO_readfile.h"
#include "BLO_readblenfile.h"

#include "BKE_blender.h"
#include "BKE_report.h"

#include "BLI_blenlib.h"

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
	memset(array, 0, sizeof(char)*4);

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
	ReportList *reports)
{
	BlendFileData *bfd= NULL;
	int fd, actualsize, datastart;
	char buf[8];

	fd= open(path, O_BINARY|O_RDONLY, 0);
	if (fd==-1) {
		BKE_report(reports, RPT_ERROR, "Unable to open");
		goto cleanup;
	}
	
	actualsize= BLI_filesize(fd);

	lseek(fd, -12, SEEK_END);

	datastart= handle_read_msb_int(fd);
	if (datastart==-1) {
		BKE_report(reports, RPT_ERROR, "Unable to read");
		goto cleanup;
	} else if (read(fd, buf, 8)!=8) {
		BKE_report(reports, RPT_ERROR, "Unable to read");
		goto cleanup;
	} else if (memcmp(buf, "BRUNTIME", 8)!=0) {
		BKE_report(reports, RPT_ERROR, "File is not a Blender file");
		goto cleanup;
	} else {	
		//printf("starting to read runtime from %s at datastart %d\n", path, datastart);
		lseek(fd, datastart, SEEK_SET);
		bfd = blo_read_blendafterruntime(fd, path, actualsize-datastart, reports);
		fd= -1;	// file was closed in blo_read_blendafterruntime()
	}
	
cleanup:
	if (fd!=-1)
		close(fd);
	
	return bfd;
}

