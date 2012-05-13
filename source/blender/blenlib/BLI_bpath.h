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
 *   so for BPath we don't need to malloc
 */

#ifndef __BLI_BPATH_H__
#define __BLI_BPATH_H__

struct ID;
struct ListBase;
struct Main;
struct ReportList;

/* Function that does something with an ID's file path. Should return 1 if the
 * path has changed, and in that case, should write the result to pathOut. */
typedef int (*BPathVisitor)(void *userdata, char *path_dst, const char *path_src);
/* Executes 'visit' for each path associated with 'id'. */
void BLI_bpath_traverse_id(struct Main *bmain, struct ID *id, BPathVisitor visit_cb, const int flag, void *userdata);
void BLI_bpath_traverse_id_list(struct Main *bmain, struct ListBase *lb, BPathVisitor visit_cb, const int flag, void *userdata);
void BLI_bpath_traverse_main(struct Main *bmain, BPathVisitor visit_cb, const int flag, void *userdata);
int  BLI_bpath_relocate_visitor(void *oldbasepath, char *path_dst, const char *path_src);

#define BLI_BPATH_TRAVERSE_ABS             (1 << 0) /* convert paths to absolute */
#define BLI_BPATH_TRAVERSE_SKIP_LIBRARY    (1 << 2) /* skip library paths */
#define BLI_BPATH_TRAVERSE_SKIP_PACKED     (1 << 3) /* skip packed data */
#define BLI_BPATH_TRAVERSE_SKIP_MULTIFILE  (1 << 4) /* skip paths where a single dir is used with an array of files, eg.
                                                     * sequence strip images and pointcache. in this case only use the first
                                                     * file, this is needed for directory manipulation functions which might
                                                     * otherwise modify the same directory multiple times */

/* high level funcs */

/* creates a text file with missing files if there are any */
void BLI_bpath_missing_files_check(struct Main *bmain, struct ReportList *reports);
void BLI_bpath_missing_files_find(struct Main *bmain, const char *searchpath, struct ReportList *reports);
void BLI_bpath_relative_convert(struct Main *bmain, const char *basedir, struct ReportList *reports);
void BLI_bpath_absolute_convert(struct Main *bmain, const char *basedir, struct ReportList *reports);

#endif // __BLI_BPATH_H__
