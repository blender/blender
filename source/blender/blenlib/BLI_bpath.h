/*
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BLI_bpath.h
 *  \ingroup bli
 *  \attention Based on ghash, difference is ghash is not a fixed size,
 *   so for BPath we dont need to malloc
 */

#ifndef BLI_BPATH_H
#define BLI_BPATH_H

struct BPathIterator;
struct ReportList;
struct Main;

void			BLI_bpathIterator_init				(struct BPathIterator **bpi, struct Main *bmain, const char *basedir, const int flag);
void			BLI_bpathIterator_free				(struct BPathIterator *bpi);
const char*		BLI_bpathIterator_getLib			(struct BPathIterator *bpi);
const char*		BLI_bpathIterator_getName			(struct BPathIterator *bpi);
int				BLI_bpathIterator_getType			(struct BPathIterator *bpi);
unsigned int	BLI_bpathIterator_getPathMaxLen		(struct BPathIterator *bpi);
const char*		BLI_bpathIterator_getBasePath		(struct BPathIterator *bpi);
void			BLI_bpathIterator_step				(struct BPathIterator *bpi);
int				BLI_bpathIterator_isDone			(struct BPathIterator *bpi);
void			BLI_bpathIterator_getPath			(struct BPathIterator *bpi, char *path);
void			BLI_bpathIterator_getPathExpanded	(struct BPathIterator *bpi, char *path_expanded);
void			BLI_bpathIterator_setPath			(struct BPathIterator *bpi, const char *path);

/* high level funcs */

/* creates a text file with missing files if there are any */
void checkMissingFiles(struct Main *bmain, struct ReportList *reports);
void makeFilesRelative(struct Main *bmain, const char *basedir, struct ReportList *reports);
void makeFilesAbsolute(struct Main *bmain, const char *basedir, struct ReportList *reports);
void findMissingFiles(struct Main *bmain, const char *str);

#define BPATH_USE_PACKED 1

#endif // BLI_BPATH_H
