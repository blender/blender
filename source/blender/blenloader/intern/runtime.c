/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
 * \file runtime.c
 * \brief This file handles the loading of .blend files embedded in runtimes
 * \ingroup blenloader
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#ifdef WIN32
#  include <io.h>       // read, open
#  include "BLI_winstuff.h"
#else // ! WIN32
#  include <unistd.h>       // read
#endif

#include "BLO_readfile.h"
#include "BLO_runtime.h"

#include "BKE_blender.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

/* Runtime reading */

static int handle_read_msb_int(int handle)
{
	unsigned char buf[4];

	if (read(handle, buf, 4) != 4)
		return -1;

	return (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + (buf[3] << 0);
}

int BLO_is_a_runtime(const char *path)
{
	int res = 0, fd = BLI_open(path, O_BINARY | O_RDONLY, 0);
	int datastart;
	char buf[8];

	if (fd == -1)
		goto cleanup;
	
	lseek(fd, -12, SEEK_END);
	
	datastart = handle_read_msb_int(fd);

	if (datastart == -1)
		goto cleanup;
	else if (read(fd, buf, 8) != 8)
		goto cleanup;
	else if (memcmp(buf, "BRUNTIME", 8) != 0)
		goto cleanup;
	else
		res = 1;

cleanup:
	if (fd != -1)
		close(fd);

	return res;	
}

BlendFileData *BLO_read_runtime(const char *path, ReportList *reports)
{
	BlendFileData *bfd = NULL;
	size_t actualsize;
	int fd, datastart;
	char buf[8];

	fd = BLI_open(path, O_BINARY | O_RDONLY, 0);

	if (fd == -1) {
		BKE_reportf(reports, RPT_ERROR, "Unable to open \"%s\": %s.", path, strerror(errno));
		goto cleanup;
	}
	
	actualsize = BLI_file_descriptor_size(fd);

	lseek(fd, -12, SEEK_END);

	datastart = handle_read_msb_int(fd);

	if (datastart == -1) {
		BKE_reportf(reports, RPT_ERROR, "Unable to read  \"%s\" (problem seeking)", path);
		goto cleanup;
	}
	else if (read(fd, buf, 8) != 8) {
		BKE_reportf(reports, RPT_ERROR, "Unable to read  \"%s\" (truncated header)", path);
		goto cleanup;
	}
	else if (memcmp(buf, "BRUNTIME", 8) != 0) {
		BKE_reportf(reports, RPT_ERROR, "Unable to read  \"%s\" (not a blend file)", path);
		goto cleanup;
	}
	else {	
		//printf("starting to read runtime from %s at datastart %d\n", path, datastart);
		lseek(fd, datastart, SEEK_SET);
		bfd = blo_read_blendafterruntime(fd, path, actualsize - datastart, reports);
		fd = -1; // file was closed in blo_read_blendafterruntime()
	}
	
cleanup:
	if (fd != -1)
		close(fd);
	
	return bfd;
}

