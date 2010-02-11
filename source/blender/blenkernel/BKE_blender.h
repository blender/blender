/**
 * blenlib/BKE_blender.h (mar-2001 nzc)
 *	
 * Blender util stuff?
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_BLENDER_H
#define BKE_BLENDER_H

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct MemFile;
struct bContext;
struct ReportList;

#define BLENDER_VERSION			250
#define BLENDER_SUBVERSION		17

#define BLENDER_MINVERSION		250
#define BLENDER_MINSUBVERSION	0

int BKE_read_file(struct bContext *C, char *dir, void *type_r, struct ReportList *reports);
int BKE_read_file_from_memory(struct bContext *C, char* filebuf, int filelength, void *type_r, struct ReportList *reports);
int BKE_read_file_from_memfile(struct bContext *C, struct MemFile *memfile, struct ReportList *reports);

void free_blender(void);
void initglobals(void);

/* load new userdef from file, exit blender */
void BKE_userdef_free(void);

/* set this callback when a UI is running */
void set_blender_test_break_cb(void (*func)(void) );
int blender_test_break(void);

/* global undo */
extern void BKE_write_undo(struct bContext *C, char *name);
extern void BKE_undo_step(struct bContext *C, int step);
extern void BKE_undo_name(struct bContext *C, const char *name);
extern void BKE_reset_undo(void);
extern char *BKE_undo_menu_string(void);
extern void BKE_undo_number(struct bContext *C, int nr);
extern void BKE_undo_save_quit(void);

#ifdef __cplusplus
}
#endif

#endif

