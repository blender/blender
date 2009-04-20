/**
 * blenlib/BKE_packedFile.h (mar-2001 nzc)
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
#ifndef BKE_PACKEDFILE_H
#define BKE_PACKEDFILE_H

#define RET_OK 0
#define RET_ERROR 1

struct PackedFile;
struct VFont;
struct bSample;
struct bSound;
struct Image;

struct PackedFile * newPackedFile(char * filename);
struct PackedFile * newPackedFileMemory(void *mem, int memlen);

int seekPackedFile(struct PackedFile * pf, int offset, int whence);
void rewindPackedFile(struct PackedFile * pf);
int readPackedFile(struct PackedFile * pf, void * data, int size);
int countPackedFiles(void);
void freePackedFile(struct PackedFile * pf);
void packAll(void);
int writePackedFile(char * filename, struct PackedFile *pf, int guimode);
int checkPackedFile(char * filename, struct PackedFile * pf);
char * unpackFile(char * abs_name, char * local_name, struct PackedFile * pf, int how);
int unpackVFont(struct VFont * vfont, int how);
int unpackSample(struct bSample *sample, int how);
int unpackImage(struct Image * ima, int how);
void unpackAll(int how);
	
#endif

