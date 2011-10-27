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

struct ReportList;
struct Main;
struct ID;

/* Function that does something with an ID's file path. Should return 1 if the
   path has changed, and in that case, should write the result to pathOut. */
typedef int (*BPathVisitor)(void *userdata, char *path_dst, const char *path_src);
/* Executes 'visit' for each path associated with 'id'. */
void bpath_traverse_id(struct Main *bmain, struct ID *id, BPathVisitor visit_cb, int flag, void *userdata);
void bpath_traverse_id_list(struct Main *bmain, struct ListBase *lb, BPathVisitor visit_cb, int flag, void *userdata);
void bpath_traverse_main(struct Main *bmain, BPathVisitor visit_cb, int flag, void *userdata);
int bpath_relocate_visitor(void *oldbasepath, char *path_dst, const char *path_src);

#define BPATH_TRAVERSE_ABS 1 /* convert paths to absolute */

/* high level funcs */

/* creates a text file with missing files if there are any */
void checkMissingFiles(struct Main *bmain, struct ReportList *reports);
void makeFilesRelative(struct Main *bmain, const char *basedir, struct ReportList *reports);
void makeFilesAbsolute(struct Main *bmain, const char *basedir, struct ReportList *reports);
void findMissingFiles(struct Main *bmain, const char *searchpath, struct ReportList *reports);

#define BPATH_USE_PACKED 1

#endif // BLI_BPATH_H
