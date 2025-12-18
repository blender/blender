/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#pragma once

#include "ED_fileselect.hh"

/* XXX could become UserPref */
#define FSMENU_RECENT_MAX 10

/**
 * Inserts a new fsmenu entry with the given \a path.
 * Duplicate entries are not added.
 * \param flag: Options for inserting the entry.
 *
 * \note The existence of `path` is *intentionally* not accessed,
 * see inline code-comments for details.
 */
void fsmenu_insert_entry(FSMenu *fsmenu,
                         enum FSMenuCategory category,
                         const char *path,
                         const char *name,
                         int icon,
                         FSMenuInsert flag);

/** Return whether the entry was created by the user and can be saved and deleted */
short fsmenu_can_save(FSMenu *fsmenu, FSMenuCategory category, int idx);

/** Removes the fsmenu entry at the given \a index. */
void fsmenu_remove_entry(FSMenu *fsmenu, FSMenuCategory category, int idx);

/**
 * Saves the 'bookmarks' to the specified file.
 * \return true on success.
 */
bool fsmenu_write_file(FSMenu *fsmenu, const char *filepath);

/** reads the 'bookmarks' from the specified file */
void fsmenu_read_bookmarks(FSMenu *fsmenu, const char *filepath);

/** adds system specific directories */
void fsmenu_read_system(FSMenu *fsmenu, int read_bookmarks);

/** Frees all the memory associated with the `fsmenu`. */
void fsmenu_free();

/** Refresh system directory menu */
void fsmenu_refresh_system_category(FSMenu *fsmenu);

/** Get active index based on given directory. */
int fsmenu_get_active_indices(FSMenu *fsmenu, FSMenuCategory category, const char *dir);

/** Add base file bookmark menu directories common to all platforms. */
void fsmenu_add_common_platform_directories(FSMenu *fsmenu);
