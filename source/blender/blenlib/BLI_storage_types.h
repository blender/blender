/**
 * blenlib/BLI_storage_types.h
 *
 * Some types for dealing with directories
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
 */
#ifndef BLI_STORAGE_TYPES_H
#define BLI_STORAGE_TYPES_H

#include <sys/stat.h>

#define HDRSIZE 512
#define NAMSIZE 200

struct header{
	char	name[NAMSIZE];
	unsigned int	size;
	unsigned int	chksum;
	char	fill[HDRSIZE-NAMSIZE-2*sizeof(unsigned int)];
};

#if defined(WIN32) && !defined(FREE_WINDOWS)
typedef unsigned int mode_t;
#endif

struct ImBuf;

struct direntry{
	char	*string;
	mode_t	type;
	char	*relname;
	char	*path;
#if (defined(WIN32) || defined(WIN64)) && (_MSC_VER>=1500)
	struct _stat64 s;
#else
	struct	stat s;
#endif
	unsigned int	flags;
	char	size[16];
	char	mode1[4];
	char	mode2[4];
	char	mode3[4];
	char	owner[16];
	char	time[8];
	char	date[16];
	char	extra[16];
	void	*poin;
	int		nr;
	struct ImBuf *image;
};

#define SELECT			1
#define HIDDEN			1
#define FIRST			1
#define DESELECT		0
#define NOT_YET			0
#define VISIBLE			0
#define LAST			0

struct dirlink
{
	struct dirlink *next,*prev;
	char *name;
};

#endif /* BLI_STORAGE_TYPES_H */

