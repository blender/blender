/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#pragma once

/* XXX could become UserPref */
#define FSMENU_RECENT_MAX 10

enum FSMenuCategory;
enum FSMenuInsert;

struct FSMenu;
struct FSMenuEntry;

/**
 * Inserts a new fsmenu entry with the given \a path.
 * Duplicate entries are not added.
 * \param flag: Options for inserting the entry.
 */
void fsmenu_insert_entry(struct FSMenu *fsmenu,
                         enum FSMenuCategory category,
                         const char *path,
                         const char *name,
                         int icon,
                         enum FSMenuInsert flag);

/** Refresh 'valid' status of given menu entry */
void fsmenu_entry_refresh_valid(struct FSMenuEntry *fsentry);

/** Return whether the entry was created by the user and can be saved and deleted */
short fsmenu_can_save(struct FSMenu *fsmenu, enum FSMenuCategory category, int idx);

/** Removes the fsmenu entry at the given \a index. */
void fsmenu_remove_entry(struct FSMenu *fsmenu, enum FSMenuCategory category, int idx);

/**
 * Saves the 'bookmarks' to the specified file.
 * \return true on success.
 */
bool fsmenu_write_file(struct FSMenu *fsmenu, const char *filepath);

/** reads the 'bookmarks' from the specified file */
void fsmenu_read_bookmarks(struct FSMenu *fsmenu, const char *filepath);

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
