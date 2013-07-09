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

/** \file DNA_packedFile_types.h
 *  \ingroup DNA
 *  \author nzc
 *  \since 12-oct-2000 nzc
 */

#ifndef __DNA_PACKEDFILE_TYPES_H__
#define __DNA_PACKEDFILE_TYPES_H__

typedef struct PackedFile {
	int   size;
	int   seek;
	void *data;
} PackedFile;

enum PF_FileStatus
{
	PF_EQUAL = 0,
	PF_DIFFERS = 1,
	PF_NOFILE = 2,

	PF_WRITE_ORIGINAL = 3,
	PF_WRITE_LOCAL = 4,
	PF_USE_LOCAL = 5,
	PF_USE_ORIGINAL = 6,
	PF_KEEP = 7,
	PF_REMOVE = 8,
	PF_NOOP = 9,

	PF_ASK = 10
};

#endif /* PACKEDFILE_TYPES_H */


