/*
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
 */

/** \file
 * \ingroup spfile
 */

#ifndef __FSMENU_H__
#define __FSMENU_H__

/* XXX could become UserPref */
#define FSMENU_RECENT_MAX 10

enum FSMenuCategory;
enum FSMenuInsert;

struct FSMenu;
struct FSMenuEntry;

/** Inserts a new fsmenu entry with the given \a path.
 * Duplicate entries are not added.
 * \param flag: Options for inserting the entry.
 */
void fsmenu_insert_entry(struct FSMenu *fsmenu,
                         enum FSMenuCategory category,
                         const char *path,
                         const char *name,
                         const enum FSMenuInsert flag);

/** Refresh 'valid' status of given menu entry */
void fsmenu_entry_refresh_valid(struct FSMenuEntry *fsentry);

/** Return whether the entry was created by the user and can be saved and deleted */
short fsmenu_can_save(struct FSMenu *fsmenu, enum FSMenuCategory category, int index);

/** Removes the fsmenu entry at the given \a index. */
void fsmenu_remove_entry(struct FSMenu *fsmenu, enum FSMenuCategory category, int index);

/** saves the 'bookmarks' to the specified file */
void fsmenu_write_file(struct FSMenu *fsmenu, const char *filename);

/** reads the 'bookmarks' from the specified file */
void fsmenu_read_bookmarks(struct FSMenu *fsmenu, const char *filename);

/** adds system specific directories */
void fsmenu_read_system(struct FSMenu *fsmenu, int read_bookmarks);

/** Free's all the memory associated with the fsmenu */
void fsmenu_free(void);

/** Refresh system directory menu */
void fsmenu_refresh_system_category(struct FSMenu *fsmenu);

/** Refresh 'valid' status of all menu entries */
void fsmenu_refresh_bookmarks_status(struct wmWindowManager *wm, struct FSMenu *fsmenu);

/** Get active index based on given directory. */
int fsmenu_get_active_indices(struct FSMenu *fsmenu,
                              enum FSMenuCategory category,
                              const char *dir);

#endif
