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
 */

#ifndef __BLI_FILEOPS_TYPES_H__
#define __BLI_FILEOPS_TYPES_H__

/** \file BLI_fileops_types.h
 *  \ingroup bli
 *  \brief Some types for dealing with directories.
 */

#include <sys/stat.h>

#if defined(WIN32)
typedef unsigned int mode_t;
#endif

#define FILELIST_DIRENTRY_SIZE_LEN  16
#define FILELIST_DIRENTRY_MODE_LEN  4
#define FILELIST_DIRENTRY_OWNER_LEN 16
#define FILELIST_DIRENTRY_TIME_LEN  8
#define FILELIST_DIRENTRY_DATE_LEN  16

struct direntry {
	mode_t  type;
	const char   *relname;
	const char   *path;
#ifdef WIN32 /* keep in sync with the definition of BLI_stat_t in BLI_fileops.h */
#  if defined(_MSC_VER)
	struct _stat64 s;
#  else
	struct _stat s;
#  endif
#else
	struct stat s;
#endif
};

struct dirlink {
	struct dirlink *next, *prev;
	char *name;
};

#endif /* __BLI_FILEOPS_TYPES_H__ */
