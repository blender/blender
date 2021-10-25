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
#ifndef __BKE_PACKEDFILE_H__
#define __BKE_PACKEDFILE_H__

/** \file BKE_packedFile.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */
#define RET_OK      0
#define RET_ERROR   1

struct ID;
struct bSound;
struct Image;
struct Main;
struct PackedFile;
struct ReportList;
struct VFont;

/* pack */
struct PackedFile *dupPackedFile(const struct PackedFile *pf_src);
struct PackedFile *newPackedFile(struct ReportList *reports, const char *filename, const char *relabase);
struct PackedFile *newPackedFileMemory(void *mem, int memlen);

void packAll(struct Main *bmain, struct ReportList *reports, bool verbose);
void packLibraries(struct Main *bmain, struct ReportList *reports);

/* unpack */
char *unpackFile(struct ReportList *reports, const char *abs_name, const char *local_name, struct PackedFile *pf, int how);
int unpackVFont(struct ReportList *reports, struct VFont *vfont, int how);
int unpackSound(struct Main *bmain, struct ReportList *reports, struct bSound *sound, int how);
int unpackImage(struct ReportList *reports, struct Image *ima, int how);
void unpackAll(struct Main *bmain, struct ReportList *reports, int how);
int unpackLibraries(struct Main *bmain, struct ReportList *reports);

int writePackedFile(struct ReportList *reports, const char *filename, struct PackedFile *pf, int guimode);

/* free */
void freePackedFile(struct PackedFile *pf);

/* info */
int countPackedFiles(struct Main *bmain);
int checkPackedFile(const char *filename, struct PackedFile *pf);

/* read */
int seekPackedFile(struct PackedFile *pf, int offset, int whence);
void rewindPackedFile(struct PackedFile *pf);
int readPackedFile(struct PackedFile *pf, void *data, int size);

/* ID should be not NULL, return 1 if there's a packed file */
bool BKE_pack_check(struct ID *id);
/* ID should be not NULL, throws error when ID is Library */
void BKE_unpack_id(struct Main *bmain, struct ID *id, struct ReportList *reports, int how);

#endif

