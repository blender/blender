/**
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
 * allocimbuf.c
 *
 * $Id$
 */

#ifdef _WIN32
#include <io.h>
#include <stddef.h>
#include <sys/types.h>
#include "mmap_win.h"
#define open _open
#define read _read
#define close _close
#endif

#include "BLI_blenlib.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

ImBuf *IMB_ibImageFromMemory(unsigned char *mem, int size, int flags)
{
	ImBuf *ibuf;
	ImFileType *type;

	if(mem == NULL) {
		printf("Error in ibImageFromMemory: NULL pointer\n");
		return NULL;
	}

	for(type=IMB_FILE_TYPES; type->is_a; type++) {
		if(type->load) {
			ibuf= type->load(mem, size, flags);
			if(ibuf) {
				if(flags & IB_premul) {
					IMB_premultiply_alpha(ibuf);
					ibuf->flags |= IB_premul;
				}

				return ibuf;
			}
		}
	}

	fprintf(stderr, "Unknown fileformat\n");
	
	return NULL;
}

ImBuf *IMB_loadifffile(int file, int flags)
{
	ImBuf *ibuf;
	unsigned char *mem;
	int size;

	if(file == -1) return 0;

	size= BLI_filesize(file);

	mem= mmap(0, size, PROT_READ, MAP_SHARED, file, 0);
	if(mem==(unsigned char*)-1) {
		fprintf(stderr, "Couldn't get mapping\n");
		return 0;
	}

	ibuf= IMB_ibImageFromMemory(mem, size, flags);

	if(munmap(mem, size))
		fprintf(stderr, "Couldn't unmap file.\n");

	return ibuf;
}

static void imb_cache_filename(char *filename, const char *name, int flags)
{
	/* read .tx instead if it exists and is not older */
	if(flags & IB_tilecache) {
		BLI_strncpy(filename, name, IB_FILENAME_SIZE);
		if(!BLI_replace_extension(filename, IB_FILENAME_SIZE, ".tx"))
			return;

		if(BLI_file_older(name, filename))
			return;
	}

	BLI_strncpy(filename, name, IB_FILENAME_SIZE);
}

ImBuf *IMB_loadiffname(const char *name, int flags)
{
	ImBuf *ibuf;
	int file, a;
	char filename[IB_FILENAME_SIZE];

	imb_cache_filename(filename, name, flags);

	file = open(filename, O_BINARY|O_RDONLY);
	if(file < 0) return 0;

	ibuf= IMB_loadifffile(file, flags);

	if(ibuf) {
		BLI_strncpy(ibuf->name, name, sizeof(ibuf->name));
		BLI_strncpy(ibuf->cachename, filename, sizeof(ibuf->cachename));
		for(a=1; a<ibuf->miptot; a++)
			BLI_strncpy(ibuf->mipmap[a-1]->cachename, filename, sizeof(ibuf->cachename));
		if(flags & IB_fields) IMB_de_interlace(ibuf);
	}

	close(file);

	return ibuf;
}

ImBuf *IMB_testiffname(char *name, int flags)
{
	ImBuf *ibuf;
	int file;
	char filename[IB_FILENAME_SIZE];

	imb_cache_filename(filename, name, flags);

	file = open(filename,O_BINARY|O_RDONLY);
	if(file < 0) return 0;

	ibuf=IMB_loadifffile(file, flags|IB_test);
	if(ibuf) {
		BLI_strncpy(ibuf->name, name, sizeof(ibuf->name));
		BLI_strncpy(ibuf->cachename, filename, sizeof(ibuf->cachename));
	}

	close(file);

	return ibuf;
}

static void imb_loadtilefile(ImBuf *ibuf, int file, int tx, int ty, unsigned int *rect)
{
	ImFileType *type;
	unsigned char *mem;
	int size;

	if(file == -1) return;

	size= BLI_filesize(file);

	mem= mmap(0, size, PROT_READ, MAP_SHARED, file, 0);
	if(mem==(unsigned char*)-1) {
		fprintf(stderr, "Couldn't get memory mapping for %s\n", ibuf->cachename);
		return;
	}

	for(type=IMB_FILE_TYPES; type->is_a; type++)
		if(type->load_tile && type->ftype(type, ibuf))
			type->load_tile(ibuf, mem, size, tx, ty, rect);

	if(munmap(mem, size))
		fprintf(stderr, "Couldn't unmap memory for %s.\n", ibuf->cachename);
}

void imb_loadtile(ImBuf *ibuf, int tx, int ty, unsigned int *rect)
{
	int file;

	file = open(ibuf->cachename, O_BINARY|O_RDONLY);
	if(file < 0) return;

	imb_loadtilefile(ibuf, file, tx, ty, rect);

	close(file);
}

