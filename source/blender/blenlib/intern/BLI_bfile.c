/* -*- indent-tabs-mode:t; tab-width:4; -*-
 *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Stichting Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 * BFILE* based abstraction for file access.
 */

#include <string.h>
#include <stdlib.h>
#ifndef WIN32
 #include <libgen.h>
 #include <unistd.h>
 #include <sys/param.h>
#else
 #include <io.h>
 #include "BLI_winstuff.h"
 static char* find_in_pathlist(char* filename, char* pathlist);
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BKE_blender.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_storage.h"
#include "BLI_bfile.h"

/* Internal bfile classification flags */
#define BCF_OPEN     (0)
#define BCF_FOPEN    (1<<0)
#define BCF_READ     (1<<1)
#define BCF_WRITE    (1<<2)
#define BCF_AT_END   (1<<3)
#define BCF_DISCARD  (1<<4)

/* Standard files names */
#define LAST_SESSION_FILE "last-session"
#define ENVIRONMENT_FILE "environment"


/* Declaration of internal functions */
static void fill_paths(BFILE *bfile, const char *path, const char *relpath);
static void free_paths(BFILE* bfile);


/*** Exported functions ***/

BFILE *BLI_bfile_fopen(const char *path, const char *mode, int bflags,
                       const char *relpath)
{
	BFILE *bfile;

	bfile = MEM_mallocN(sizeof(BFILE), "bfile-fopen");
	bfile->classf = BCF_FOPEN;
	bfile->uflags = bflags;

	/* From fopen() doc, we can guess some logic:
	r  BCF_READ
	r+ BCF_READ | BCF_WRITE
	w  BCF_DISCARD | BCF_WRITE
	w+ BCF_DISCARD | BCF_WRITE | BCF_READ
	a  BCF_AT_END | BCF_WRITE
	a+ BCF_AT_END | BCF_WRITE | BCF_READ
	*/
	if (strchr(mode, 'r'))
		bfile->classf |= BCF_READ;
	if (strchr(mode, 'w'))
		bfile->classf |= (BCF_DISCARD | BCF_WRITE);
	if (strchr(mode, 'a'))
		bfile->classf |= (BCF_AT_END | BCF_WRITE);
	if (strchr(mode, '+'))
		bfile->classf |= (BCF_READ | BCF_WRITE);

	fill_paths(bfile, path, relpath);

	bfile->stream = fopen(bfile->tpath, mode);
	if (!(bfile->stream)) {
		free_paths(bfile);
		MEM_freeN(bfile);
		return NULL;
	}

	bfile->fd = fileno(bfile->stream);

	return bfile;
}


BFILE *BLI_bfile_open(const char *pathname, int flags, int bflags,
                      const char *relpath)
{
	BFILE *bfile;
	char fopen_mode[3];

	bfile = MEM_mallocN(sizeof(BFILE), "bfile-open");
	bfile->classf = BCF_OPEN;
	bfile->uflags = bflags;

	/* Easy mapping for open() */
	if (flags & O_RDONLY)
		bfile->classf |= BCF_READ;
	if (flags & O_WRONLY)
		bfile->classf |= BCF_WRITE;
	if (flags & O_RDWR)
		bfile->classf |= (BCF_READ | BCF_WRITE);
	if (flags & O_APPEND)
		bfile->classf |= BCF_AT_END;
	if (flags & O_TRUNC)
		bfile->classf |= BCF_DISCARD;

	fill_paths(bfile, pathname, relpath);

	bfile->fd = open(bfile->tpath, flags);
	if (bfile->fd == -1) {
		free_paths(bfile);
		MEM_freeN(bfile);
		return NULL;
	}

	fopen_mode[0] = 'r';
	fopen_mode[1] = '\0';
	fopen_mode[2] = '\0';
	if (bfile->classf & BCF_DISCARD) {
		fopen_mode[0] = 'w';
		if (bfile->classf & BCF_READ) {
			fopen_mode[1] = '+';
		}
	} else if (bfile->classf & BCF_AT_END) {
		fopen_mode[0] = 'a';
		if (bfile->classf & BCF_READ) {
			fopen_mode[1] = '+';
		}
	} else if (bfile->classf & BCF_WRITE) {
		fopen_mode[1] = '+';
	}

	bfile->stream = fdopen(bfile->fd, fopen_mode); /* MSWindows _fdopen? */
	if (!(bfile->stream)) {
		free_paths(bfile);
		MEM_freeN(bfile);
		return NULL;
	}

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


size_t BLI_bfile_fwrite(const void *ptr, size_t size, size_t nmemb,
                        BFILE *f)
{
	size_t ret;

	if (f == NULL)
		return 0;

	ret = fwrite(ptr, size, nmemb, f->stream);
	if (ret <= 0) {
		f->error = 1;
	}

	return ret;
}


size_t BLI_bfile_fread(void *ptr, size_t size, size_t nmemb, BFILE *f) {
	size_t ret;

	if (f == NULL)
		return 0;

	ret = fread(ptr, size, nmemb, f->stream);
	if ((ret <= 0) && ferror(f->stream)) {
		f->error = 1;
	}

	return ret;
}


void BLI_bfile_close(BFILE *bfile) {
	if ((bfile->classf | BCF_WRITE) &&
	    !(bfile->uflags | BFILE_RAW)) {
		int error;
		/* Make sure data is on disk */
		error = fsync(bfile->fd);
		/* fsync the directory too? */
		/* Move to final name if no errors */
		if (!(bfile->error) && !error) {
			rename(bfile->tpath, bfile->fpath);
		}
	}

	/* Normal close */

	/* Cleanup */
	free_paths(bfile);
	MEM_freeN(bfile);
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


/*** Internal functions ***/

/**
 Return a full path if the filename exists when combined
 with any item from pathlist. Or NULL otherwise.
 */
#ifdef WIN32
 #define SEPARATOR ';'
#else
 #define SEPARATOR ':'
#endif

#ifdef WIN32
static char* find_in_pathlist(char* filename, char* pathlist) {
	char first[FILE_MAX + 10];
	char* rest = NULL;

	/* Separate first path from rest, use typical separator for current OS */
	rest = strchr(pathlist, SEPARATOR);
	if (rest) {
		strncpy(first, pathlist, rest - pathlist);
		first[rest - pathlist] = '\0';
		/* Skip the separator so it becomes a valid new pathlist */
		rest++;
	} else {
		strcpy(first, pathlist);
	}

	/* Check if combination exists */
	BLI_add_slash(first);
	strcat(first, filename);
	if (BLI_exist(first)) {
		return strdup(first);
	}

	/* First path failed, try with rest of paths if possible */
	if (rest) {
		return find_in_pathlist(filename, rest);
	} else {
		return NULL;
	}
}
#endif

/**
 Setup fpath and tpath based in the needs of the bfile.
 */
static void fill_paths(BFILE *bfile, const char *path, const char *relpath) {
	char* source_path = NULL;
	char* temp_path = NULL;
	int bflags = bfile->uflags;

	if (bflags & BFILE_NORMAL || bflags & BFILE_RAW) {
//		bfile->fpath is path with // replaced
	}
	if (bflags & BFILE_TEMP) {
		temp_path = MEM_mallocN(MAXPATHLEN, "bfile-fpath-1");
		snprintf(temp_path, MAXPATHLEN, "%s/%s", getenv("BLENDER_TEMP"), path);
		bfile->fpath = temp_path;
	}
	if (bflags & (BFILE_CONFIG_BASE | BFILE_CONFIG_DATAFILES |
	              BFILE_CONFIG_PYTHON | BFILE_CONFIG_PLUGINS)) {
// evars
//		bfile->fpath is userdir+version+path
//		source_path is first hit in (if using fallback to older versions)
//		    userdir+curversion+path (... userdir+limitversion+path) sysdir+path
//		(limitversion is based in path, using some kind of regex or "tables")
	}

	if (bfile->classf & BCF_WRITE && !(bflags & BFILE_RAW)) {
		/* Generate random named path */
		temp_path = MEM_mallocN(MAXPATHLEN, "bfile-fpath-2");
		snprintf(temp_path, MAXPATHLEN, "%s.XXXXXX", path);
		bfile->fd = mkstemp(temp_path);
		bfile->tpath = temp_path;
		/* It will be reopened in upper levels, later */
		close(bfile->fd);
		if (!(bfile->classf & BCF_DISCARD)) {
			/* Copy original data into temp location */
			if (source_path) {
				BLI_copy_fileops(source_path, bfile->tpath);
			} else {
				BLI_copy_fileops(bfile->fpath, bfile->tpath);
			}
		}
	} else {
		bfile->tpath = bfile->fpath;
	}
}


/**
 Free memory used for path strings.
 */
static void free_paths(BFILE* bfile) {
	if (bfile->fpath) {
		MEM_freeN(bfile->fpath);
	}
	if (bfile->tpath) {
		MEM_freeN(bfile->tpath);
	}
}
