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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_fileselect.h
 *  \ingroup editors
 */

#ifndef __ED_FILESELECT_H__
#define __ED_FILESELECT_H__

struct ARegion;
struct FileSelectParams;
struct ScrArea;
struct SpaceFile;
struct bContext;
struct wmWindowManager;

#define FILE_LAYOUT_HOR 1
#define FILE_LAYOUT_VER 2

#define MAX_FILE_COLUMN 4

typedef enum FileListColumns {
	COLUMN_NAME = 0,
	COLUMN_DATE,
	COLUMN_TIME,
	COLUMN_SIZE,
} FileListColumns;

typedef struct FileLayout {
	/* view settings - XXX - move into own struct */
	int prv_w;
	int prv_h;
	int tile_w;
	int tile_h;
	int tile_border_x;
	int tile_border_y;
	int prv_border_x;
	int prv_border_y;
	int rows;
	int columns;
	int width;
	int height;
	int flag;
	int dirty;
	int textheight;
	float column_widths[MAX_FILE_COLUMN];

	/* When we change display size, we may have to update static strings like size of files... */
	short curr_size;
} FileLayout;

typedef struct FileSelection {
	int first;
	int last;
} FileSelection;

struct rcti;

struct FileSelectParams *ED_fileselect_get_params(struct SpaceFile *sfile);

short ED_fileselect_set_params(struct SpaceFile *sfile);

void ED_fileselect_reset_params(struct SpaceFile *sfile);


void ED_fileselect_init_layout(struct SpaceFile *sfile, struct ARegion *ar);


FileLayout *ED_fileselect_get_layout(struct SpaceFile *sfile, struct ARegion *ar);

int ED_fileselect_layout_numfiles(FileLayout *layout, struct ARegion *ar);
int ED_fileselect_layout_offset(FileLayout *layout, int x, int y);
FileSelection ED_fileselect_layout_offset_rect(FileLayout *layout, const struct rcti *rect);

void ED_fileselect_layout_tilepos(FileLayout *layout, int tile, int *x, int *y);

void ED_operatormacros_file(void);

void ED_fileselect_clear(struct wmWindowManager *wm, struct ScrArea *sa, struct SpaceFile *sfile);

void ED_fileselect_exit(struct wmWindowManager *wm, struct ScrArea *sa, struct SpaceFile *sfile);

int ED_path_extension_type(const char *path);
int ED_file_extension_icon(const char *path);

void ED_file_read_bookmarks(void);

void ED_file_change_dir(struct bContext *C);

/* File menu stuff */

typedef enum FSMenuCategory {
	FS_CATEGORY_SYSTEM,
	FS_CATEGORY_SYSTEM_BOOKMARKS,
	FS_CATEGORY_BOOKMARKS,
	FS_CATEGORY_RECENT
} FSMenuCategory;

typedef enum FSMenuInsert {
	FS_INSERT_SORTED = (1 << 0),
	FS_INSERT_SAVE   = (1 << 1),
	FS_INSERT_FIRST  = (1 << 2),  /* moves the item to the front of the list when its not already there */
	FS_INSERT_LAST   = (1 << 3),  /* just append to preseve delivered order */
} FSMenuInsert;

struct FSMenu;
struct FSMenuEntry;

struct FSMenu *ED_fsmenu_get(void);
struct FSMenuEntry *ED_fsmenu_get_category(struct FSMenu *fsmenu, FSMenuCategory category);
void ED_fsmenu_set_category(struct FSMenu *fsmenu, FSMenuCategory category, struct FSMenuEntry *fsm_head);

int ED_fsmenu_get_nentries(struct FSMenu *fsmenu, FSMenuCategory category);

struct FSMenuEntry *ED_fsmenu_get_entry(struct FSMenu *fsmenu, FSMenuCategory category, int index);

char *ED_fsmenu_entry_get_path(struct FSMenuEntry *fsentry);
void ED_fsmenu_entry_set_path(struct FSMenuEntry *fsentry, const char *path);

char *ED_fsmenu_entry_get_name(struct FSMenuEntry *fsentry);
void ED_fsmenu_entry_set_name(struct FSMenuEntry *fsentry, const char *name);

#endif /* __ED_FILESELECT_H__ */
