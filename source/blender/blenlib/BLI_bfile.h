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
 * BFILE* based abstraction of file access.
 */

#ifndef BLI_BFILE_H
#define BLI_BFILE_H

/* For fopen's FILE */
#include <stdio.h>

/**
 Defines for the bflags param.
 */
/* Special handling: */
/* For "symmetry" of flags */
#define BFILE_NORMAL (0)
/* No supervision, just translate // if needed, RISKY */
#define BFILE_RAW    (1<<0)
/* Path is based in env vars specified by "envvars" */
#define BFILE_CONFIG (1<<1)
/* Path is for current session temp files */
#define BFILE_TEMP   (1<<2)

/* Config handling, special cases: */
#define BFILE_USERONLY (1<<3)
#define BFILE_SYSONLY  (1<<4)

/* Compression to apply on close: */
#define BFILE_GZIP (1<<5)

/**
 For the envvars param.
 */
typedef enum BEnvVarFamilies {
	BENV_NONE,
	BENV_BASE,
	BENV_DATAFILES,
	BENV_PYTHON,
	BENV_PLUGINS
} BEnvVarFam;

/**
 File descriptor for Blender abstracted file access.
 */
typedef struct {
	FILE *stream;
	int fd;

	/* Anything below should not be touched directly */
	int uflags;       /* Special options requested by upper level, copy of bflags */
	BEnvVarFam evars; /* What kind of file, describe the env vars to use */
	char *fpath;      /* Final/requested path name */
	char *tpath;      /* Temp path name if applicable */
	int classf;       /* Own flags, common classification of open and fopen */
	int error;        /* An op caused an error, unsafe to replace older files */
} BFILE;

/**
 Open a BFILE* with fopen()-like syntax.
 */
BFILE *BLI_bfile_fopen(const char *path, const char *mode, int bflags, BEnvVarFam envvars);

/**
 Open a BFILE* with open()-like syntax.
 */
BFILE *BLI_bfile_open(const char *pathname, int flags, int bflags, BEnvVarFam envvars);

/**
 Get the FILE* associated with the BFILE*.
 */
FILE *BLI_bfile_file_from_bfile(BFILE *bfile);

/**
 Get the fd associated with the BFILE*.
 */
int BLI_bfile_fd_from_bfile(BFILE *bfile);

/**
 write()-like using BFILE*.
 */
ssize_t BLI_bfile_write(BFILE *f, const void *buf, size_t count);

/**
 read()-like using BFILE*.
 */
ssize_t BLI_bfile_read(BFILE *f, void *buf, size_t count);

/**
 fwrite()-like using BFILE*.
 */
size_t BLI_bfile_fwrite(const void *ptr, size_t size, size_t nmemb, BFILE *f);

/**
 fread()-like using BFILE*.
 */
size_t BLI_bfile_fread(void *ptr, size_t size, size_t nmemb, BFILE *f);

/**
 Close a BFILE, to match close() and fclose().
 */
void BLI_bfile_close(BFILE *bfile);

/**
 Clear error status.
 Call it only if the error has been really handled.
 */
void BLI_bfile_clear_error(BFILE *bfile);

/**
 Set the error status.
 Call it to mark writing by a 3rd party failed (libjpeg reported error, ie).
 */
void BLI_bfile_set_error(BFILE *bfile, int error);

/*
TODO
Maybe also provide more OS/libc things like:
fflush
fprintf and related
fscanf
fgetc/fputc and related
fseek and related

Probably good to do:
readdir (compacted list showing all files for a "directory" (user versions on top of system's))
*/

#endif /* ifndef BLI_BFILE_H */
