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
#ifndef BKE_PACKEDFILE_H
#define BKE_PACKEDFILE_H

#define RET_OK		0
#define RET_ERROR	1

struct bSound;
struct Image;
struct Main;
struct PackedFile;
struct ReportList;
struct VFont;

/* pack */
struct PackedFile *newPackedFile(struct ReportList *reports, char *filename);
struct PackedFile *newPackedFileMemory(void *mem, int memlen);

void packAll(struct Main *bmain, struct ReportList *reports);

/* unpack */
char *unpackFile(struct ReportList *reports, char *abs_name, char *local_name, struct PackedFile *pf, int how);
int unpackVFont(struct ReportList *reports, struct VFont *vfont, int how);
int unpackSound(struct ReportList *reports, struct bSound *sound, int how);
int unpackImage(struct ReportList *reports, struct Image *ima, int how);
void unpackAll(struct Main *bmain, struct ReportList *reports, int how);

int writePackedFile(struct ReportList *reports, char *filename, struct PackedFile *pf, int guimode);

/* free */
void freePackedFile(struct PackedFile *pf);

/* info */
int countPackedFiles(struct Main *bmain);
int checkPackedFile(char *filename, struct PackedFile *pf);

/* read */
int seekPackedFile(struct PackedFile *pf, int offset, int whence);
void rewindPackedFile(struct PackedFile *pf);
int readPackedFile(struct PackedFile *pf, void *data, int size);

#endif

