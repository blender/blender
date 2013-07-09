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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BKE_bpath.h
 *  \ingroup bke
 *  \attention Based on ghash, difference is ghash is not a fixed size,
 *   so for BPath we don't need to malloc
 */

#ifndef __BKE_BPATH_H__
#define __BKE_BPATH_H__

struct ID;
struct ListBase;
struct Main;
struct ReportList;

/* Function that does something with an ID's file path. Should return 1 if the
 * path has changed, and in that case, should write the result to pathOut. */
typedef bool (*BPathVisitor)(void *userdata, char *path_dst, const char *path_src);
/* Executes 'visit' for each path associated with 'id'. */
void BKE_bpath_traverse_id(struct Main *bmain, struct ID *id, BPathVisitor visit_cb, const int flag, void *userdata);
void BKE_bpath_traverse_id_list(struct Main *bmain, struct ListBase *lb, BPathVisitor visit_cb, const int flag, void *userdata);
void BKE_bpath_traverse_main(struct Main *bmain, BPathVisitor visit_cb, const int flag, void *userdata);
bool BKE_bpath_relocate_visitor(void *oldbasepath, char *path_dst, const char *path_src);

/* Functions for temp backup/restore of paths, path count must NOT change */
void *BKE_bpath_list_backup(struct Main *bmain, const int flag);
void  BKE_bpath_list_restore(struct Main *bmain, const int flag, void *ls_handle);
void  BKE_bpath_list_free(void *ls_handle);

#define BKE_BPATH_TRAVERSE_ABS             (1 << 0) /* convert paths to absolute */
#define BKE_BPATH_TRAVERSE_SKIP_LIBRARY    (1 << 2) /* skip library paths */
#define BKE_BPATH_TRAVERSE_SKIP_PACKED     (1 << 3) /* skip packed data */
#define BKE_BPATH_TRAVERSE_SKIP_MULTIFILE  (1 << 4) /* skip paths where a single dir is used with an array of files, eg.
                                                     * sequence strip images and pointcache. in this case only use the first
                                                     * file, this is needed for directory manipulation functions which might
                                                     * otherwise modify the same directory multiple times */

/* high level funcs */

/* creates a text file with missing files if there are any */
void BKE_bpath_missing_files_check(struct Main *bmain, struct ReportList *reports);
void BKE_bpath_missing_files_find(struct Main *bmain, const char *searchpath, struct ReportList *reports,
                                  const bool find_all);
void BKE_bpath_relative_convert(struct Main *bmain, const char *basedir, struct ReportList *reports);
void BKE_bpath_absolute_convert(struct Main *bmain, const char *basedir, struct ReportList *reports);

#endif  /* __BKE_BPATH_H__ */
