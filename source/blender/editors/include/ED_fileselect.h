/**
 * $Id$
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_FILES_H
#define ED_FILES_H

struct SpaceFile;
struct ARegion;
struct FileSelectParams;
struct bContext;

#define FILE_LAYOUT_HOR 1
#define FILE_LAYOUT_VER 2

#define MAX_FILE_COLUMN 8

typedef enum FileListColumns {
	COLUMN_NAME = 0,
	COLUMN_DATE,
	COLUMN_TIME,
	COLUMN_SIZE,
	COLUMN_MODE1,
	COLUMN_MODE2,
	COLUMN_MODE3,
	COLUMN_OWNER
} FileListColumns;

typedef struct FileLayout
{
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
} FileLayout;

struct FileSelectParams* ED_fileselect_get_params(struct SpaceFile *sfile);

short ED_fileselect_set_params(struct SpaceFile *sfile);

void ED_fileselect_reset_params(struct SpaceFile *sfile);


void ED_fileselect_init_layout(struct SpaceFile *sfile, struct ARegion *ar);


FileLayout* ED_fileselect_get_layout(struct SpaceFile *sfile, struct ARegion *ar);

int ED_fileselect_layout_numfiles(FileLayout* layout, struct ARegion *ar);
int ED_fileselect_layout_offset(FileLayout* layout, int clamp_bounds, int x, int y);

void ED_fileselect_layout_tilepos(FileLayout* layout, int tile, int *x, int *y);

void ED_operatormacros_file(void);

void ED_fileselect_clear(struct bContext *C, struct SpaceFile *sfile);

void ED_fileselect_exit(struct bContext *C, struct SpaceFile *sfile);

#endif /* ED_FILES_H */

