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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * external writefile function prototypes
 */

#ifndef BLO_UNDOFILE_H
#define BLO_UNDOFILE_H

typedef struct {
	void *next, *prev;
	
	char *buf;
	unsigned int ident, size;
	
} MemFileChunk;

typedef struct MemFile {
	ListBase chunks;
	unsigned int size;
} MemFile;

/* actually only used writefile.c */
extern void add_memfilechunk(MemFile *compare, MemFile *current, char *buf, unsigned int size);

/* exports */
extern void BLO_free_memfile(MemFile *memfile);
extern void BLO_merge_memfile(MemFile *first, MemFile *second);

#endif

