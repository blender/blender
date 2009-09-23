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
 * The Original Code is Copyright (C) 2009 by Stichting Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 * BFILE* based abstraction for file access.
 */

#include <string.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "MEM_guardedalloc.h"

#include "BLI_bfile.h"

// This would provide config paths and their oldest viable version
// so if there is an uncompatible change, user's old versions are not loaded
//#include "bfile_tables.h"

/* Internal bfile type flags */
#define BTF_OPEN     (0)
#define BTF_FOPEN    (1<<0)
#define BTF_READ     (1<<1)
#define BTF_WRITE    (1<<2)
#define BTF_AT_END   (1<<3)
#define BTF_DISCARD  (1<<4)


void fill_paths(BFILE *bfile, const char *path) {
	char* source_path = NULL;
	int bflags = bfile->uflags;

	if(bflags & BFILE_NORMAL || bflags & BFILE_RAW) {
//		bfile->fpath is path with // replaced
	}
	if(bflags & BFILE_TEMP) {
//		bfile->fpath is tempdir+path
	}
	if(bflags & BFILE_CONFIG) {
//		bfile->fpath is userdir+version+path
//		source_path is first hit in (if using fallback to older versions)
//		    userdir+curversion+path (... userdir+limitversion+path) sysdir+path
//		(limitversion is based in path, using some kind of regex or "tables")
	}

	if(bfile->type & BTF_WRITE && !(bflags & BFILE_RAW)) {
		/* Generate temp path */
		// bfile->tpath is fpath+randstring
		if(!(bfile->type & BTF_DISCARD)) {
			/* Copy data to tpath */
			if(source_path) {
				// copy it from older version or sys version
			}
		}
	} else {
		bfile->tpath = bfile->fpath;
	}
}

BFILE *BLI_bfile_fopen(const char *path, const char *mode, int bflags) {
	BFILE *bfile;

	bfile = MEM_mallocN(sizeof(BFILE), "bfile-fopen");
	bfile->type = BTF_FOPEN;
	bfile->uflags = bflags;

	/* From fopen() doc, we can guess some logic:
	r  BTF_READ
	r+ BTF_READ | BTF_WRITE
	w  BTF_DISCARD | BTF_WRITE
	w+ BTF_DISCARD | BTF_WRITE | BTF_READ
	a  BTF_AT_END | BTF_WRITE
	a+ BTF_AT_END | BTF_WRITE | BTF_READ
	*/
	if(strchr(mode, 'r'))
		bfile->type |= BTF_READ;
	if(strchr(mode, 'w'))
		bfile->type |= (BTF_DISCARD | BTF_WRITE);
	if(strchr(mode, 'a'))
		bfile->type |= (BTF_AT_END | BTF_WRITE);
	if(strchr(mode, '+'))
		bfile->type |= (BTF_READ | BTF_WRITE);

	fill_paths(bfile, path);

	bfile->stream = fopen(bfile->tpath, mode);
	// detect failed fopen
	bfile->fd = fileno(bfile->stream);
	return bfile;
}


BFILE *BLI_bfile_open(const char *pathname, int flags, int bflags) {
	BFILE *bfile;

	bfile = MEM_mallocN(sizeof(BFILE), "bfile-open");
	bfile->type = BTF_OPEN;
	bfile->uflags = bflags;

	/* Easy mapping for open() */
	if(flags & O_RDONLY)
		bfile->type |= BTF_READ;
	if(flags & O_WRONLY)
		bfile->type |= BTF_WRITE;
	if(flags & O_RDWR)
		bfile->type |= (BTF_READ | BTF_WRITE);
	if(flags & O_APPEND)
		bfile->type |= BTF_AT_END;
	if(flags & O_TRUNC)
		bfile->type |= BTF_DISCARD;

	fill_paths(bfile, pathname);

	bfile->fd = open(bfile->tpath, flags);
	// detect failed open
//	bfile->stream = fdopen(bfile->fd, XXX); /* MSWindows _fdopen? */
	return bfile;
}


FILE *BLI_bfile_file_from_bfile(BFILE *bfile) {
	return bfile->stream;
}


int BLI_bfile_fd_from_bfile(BFILE *bfile) {
	return bfile->fd;
}


ssize_t BLI_bfile_write(BFILE *f, const void *buf, size_t count) {
	ssize_t ret;

	ret = write((f->fd), buf, count);
	if (ret == -1) {
		f->error = 1;
	}

	return ret;
}


ssize_t BLI_bfile_read(BFILE *f, void *buf, size_t count) {
	ssize_t ret;

	ret = read((f->fd), buf, count);
	if (ret == -1) {
		f->error = 1;
	}

	return ret;
}


size_t BLI_bfile_fwrite(const void *ptr, size_t size, size_t nmemb, BFILE *f) {
	size_t ret;

	ret = fwrite(ptr, size, nmemb, f->stream);
	if (ret < 0) {
		f->error = 1;
	}

	return ret;
}


size_t BLI_bfile_fread(void *ptr, size_t size, size_t nmemb, BFILE *f) {
	size_t ret;

	ret = fread(ptr, size, nmemb, f->stream);
	if ((ret < 0) && ferror(f->stream)) {
		f->error = 1;
	}

	return ret;
}


void BLI_bfile_close(BFILE *bfile) {
	if((bfile->type | BTF_WRITE) &&
	   !(bfile->uflags | BFILE_RAW)) {
		/* Make sure data is on disk */
		/* Move to final name if no errors */
	}

	/* Normal close */

	/* Cleanup */
	if(bfile->fpath) {
		MEM_freeN(bfile->fpath);
	}
	if(bfile->tpath) {
		MEM_freeN(bfile->tpath);
	}
}


void BLI_bfile_clear_error(BFILE *bfile) {
	bfile->error = 0;
}


void BLI_bfile_set_error(BFILE *bfile, int error) {
	/* No cheating, use clear_error() for 0 */
	if (error) {
		bfile->error = error;
	}
}
