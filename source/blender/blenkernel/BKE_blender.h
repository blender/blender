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
 */
#ifndef BKE_BLENDER_H
#define BKE_BLENDER_H

/** \file BKE_blender.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 *  \brief Blender util stuff
 */

#ifdef __cplusplus
extern "C" {
#endif

/* these lines are grep'd, watch out for our not-so-awesome regex
 * and keep comment above the defines.
 * Use STRINGIFY() rather than defining with quotes */
#define BLENDER_VERSION			261
#define BLENDER_SUBVERSION		3

#define BLENDER_MINVERSION		250
#define BLENDER_MINSUBVERSION	0

/* used by packaging tools */
		/* can be left blank, otherwise a,b,c... etc with no quotes */
#define BLENDER_VERSION_CHAR	
		/* alpha/beta/rc/release, docs use this */
#define BLENDER_VERSION_CYCLE	alpha

extern char versionstr[]; /* from blender.c */

struct ListBase;
struct MemFile;
struct bContext;
struct ReportList;
struct Scene;
struct Main;

int BKE_read_file(struct bContext *C, const char *filepath, struct ReportList *reports);

#define BKE_READ_FILE_FAIL				0 /* no load */
#define BKE_READ_FILE_OK				1 /* OK */
#define BKE_READ_FILE_OK_USERPREFS		2 /* OK, and with new user settings */

int BKE_read_file_from_memory(struct bContext *C, char* filebuf, int filelength, struct ReportList *reports);
int BKE_read_file_from_memfile(struct bContext *C, struct MemFile *memfile, struct ReportList *reports);

void free_blender(void);
void initglobals(void);

/* load new userdef from file, exit blender */
void BKE_userdef_free(void);

/* set this callback when a UI is running */
void set_blender_test_break_cb(void (*func)(void) );
int blender_test_break(void);

/* global undo */
extern void BKE_write_undo(struct bContext *C, const char *name);
extern void BKE_undo_step(struct bContext *C, int step);
extern void BKE_undo_name(struct bContext *C, const char *name);
extern int BKE_undo_valid(const char *name);
extern void BKE_reset_undo(void);
extern char *BKE_undo_menu_string(void);
extern void BKE_undo_number(struct bContext *C, int nr);
extern const char *BKE_undo_get_name(int nr, int *active);
extern void BKE_undo_save_quit(void);
extern struct Main *BKE_undo_get_main(struct Scene **scene);

#ifdef __cplusplus
}
#endif

#endif

