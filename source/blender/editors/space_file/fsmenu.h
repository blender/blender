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
 * 
 */

/** \file blender/editors/space_file/fsmenu.h
 *  \ingroup spfile
 */


#ifndef __FSMENU_H__
#define __FSMENU_H__

/* XXX could become UserPref */
#define FSMENU_RECENT_MAX 10

typedef enum FSMenuCategory {
	FS_CATEGORY_SYSTEM,
	FS_CATEGORY_SYSTEM_BOOKMARKS,
	FS_CATEGORY_BOOKMARKS,
	FS_CATEGORY_RECENT
} FSMenuCategory;

typedef enum FSMenuInsert {
	FS_INSERT_SORTED = (1 << 0),
	FS_INSERT_SAVE   = (1 << 1),
	FS_INSERT_FIRST  = (1 << 2),   /* moves the item to the front of the list when its not already there */
	FS_APPEND_LAST  = (1 << 3)   /*just append to preseve delivered order */
} FSMenuInsert;

struct FSMenu;

struct FSMenu *fsmenu_get(void);

/** Returns the number of entries in the Fileselect Menu */
int     fsmenu_get_nentries(struct FSMenu *fsmenu, FSMenuCategory category);

/** Returns the fsmenu entry at \a index (or NULL if a bad index)
 * or a separator.
 */
char *fsmenu_get_entry(struct FSMenu *fsmenu, FSMenuCategory category, int index);

/** Inserts a new fsmenu entry with the given \a path.
 * Duplicate entries are not added.
 * \param flag Options for inserting the entry.
 */
void    fsmenu_insert_entry(struct FSMenu *fsmenu, FSMenuCategory category, const char *path, const FSMenuInsert flag);

/** Return whether the entry was created by the user and can be saved and deleted */
short   fsmenu_can_save(struct FSMenu *fsmenu, FSMenuCategory category, int index);

/** Removes the fsmenu entry at the given \a index. */
void    fsmenu_remove_entry(struct FSMenu *fsmenu, FSMenuCategory category, int index);

/** saves the 'bookmarks' to the specified file */
void    fsmenu_write_file(struct FSMenu *fsmenu, const char *filename);
	
/** reads the 'bookmarks' from the specified file */
void    fsmenu_read_bookmarks(struct FSMenu *fsmenu, const char *filename);

/** adds system specific directories */
void    fsmenu_read_system(struct FSMenu *fsmenu, int read_bookmarks);

/** Free's all the memory associated with the fsmenu */
void    fsmenu_free(void);

/** Refresh system directory menu */
void    fsmenu_refresh_system_category(struct FSMenu *fsmenu);

#endif

