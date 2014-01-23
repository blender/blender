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
#ifndef __BKE_BLENDER_H__
#define __BKE_BLENDER_H__

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
#define BLENDER_VERSION         269
#define BLENDER_SUBVERSION      10
/* 262 was the last editmesh release but it has compatibility code for bmesh data */
#define BLENDER_MINVERSION      262
#define BLENDER_MINSUBVERSION   0

/* used by packaging tools */
/* can be left blank, otherwise a,b,c... etc with no quotes */
#define BLENDER_VERSION_CHAR   
/* alpha/beta/rc/release, docs use this */
#define BLENDER_VERSION_CYCLE   alpha

extern char versionstr[]; /* from blender.c */

struct ListBase;
struct MemFile;
struct bContext;
struct ReportList;
struct Scene;
struct Main;
struct ID;

int BKE_read_file(struct bContext *C, const char *filepath, struct ReportList *reports);

#define BKE_READ_FILE_FAIL              0 /* no load */
#define BKE_READ_FILE_OK                1 /* OK */
#define BKE_READ_FILE_OK_USERPREFS      2 /* OK, and with new user settings */

int BKE_read_file_from_memory(struct bContext *C, const void *filebuf,
	int filelength, struct ReportList *reports, int update_defaults);
int BKE_read_file_from_memfile(struct bContext *C, struct MemFile *memfile,
	struct ReportList *reports);

int BKE_read_file_userdef(const char *filepath, struct ReportList *reports);
int BKE_write_file_userdef(const char *filepath, struct ReportList *reports);

void free_blender(void);
void initglobals(void);

/* load new userdef from file, exit blender */
void BKE_userdef_free(void);
/* handle changes in userdef */
void BKE_userdef_state(void);
	
/* set this callback when a UI is running */
void set_blender_test_break_cb(void (*func)(void) );
int blender_test_break(void);

#define BKE_UNDO_STR_MAX 64

/* global undo */
extern void BKE_write_undo(struct bContext *C, const char *name);
extern void BKE_undo_step(struct bContext *C, int step);
extern void BKE_undo_name(struct bContext *C, const char *name);
extern int BKE_undo_valid(const char *name);
extern void BKE_reset_undo(void);
extern void BKE_undo_number(struct bContext *C, int nr);
extern const char *BKE_undo_get_name(int nr, int *active);
extern int BKE_undo_save_file(const char *filename);
extern struct Main *BKE_undo_get_main(struct Scene **scene);

/* copybuffer */
void BKE_copybuffer_begin(struct Main *bmain);
void BKE_copybuffer_tag_ID(struct ID *id);
int BKE_copybuffer_save(const char *filename, struct ReportList *reports);
int BKE_copybuffer_paste(struct bContext *C, const char *libname, struct ReportList *reports);

#ifdef __cplusplus
}
#endif

#endif

