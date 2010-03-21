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
#ifndef ED_FILE_INTERN_H
#define ED_FILE_INTERN_H

/* internal exports only */

struct ARegion;
struct ARegionType;
struct SpaceFile;

/* file_ops.c */
struct ARegion *file_buttons_region(struct ScrArea *sa);

/* file_draw.c */
#define TILE_BORDER_X 8
#define TILE_BORDER_Y 8

void file_draw_buttons(const bContext *C, ARegion *ar);
void file_calc_previews(const bContext *C, ARegion *ar);
void file_draw_previews(const bContext *C, ARegion *ar);
void file_draw_list(const bContext *C, ARegion *ar);

/* file_ops.h */
struct wmOperatorType;
struct wmOperator;
struct wmEvent;
void FILE_OT_highlight(struct wmOperatorType *ot);
void FILE_OT_select(struct wmOperatorType *ot);
void FILE_OT_select_all_toggle(struct wmOperatorType *ot);
void FILE_OT_select_border(struct wmOperatorType *ot);
void FILE_OT_select_bookmark(struct wmOperatorType *ot);
void FILE_OT_bookmark_add(struct wmOperatorType *ot);
void FILE_OT_delete_bookmark(struct wmOperatorType *ot);
void FILE_OT_hidedot(struct wmOperatorType *ot);
void FILE_OT_execute(struct wmOperatorType *ot);
void FILE_OT_cancel(struct wmOperatorType *ot);
void FILE_OT_parent(struct wmOperatorType *ot);
void FILE_OT_directory_new(struct wmOperatorType *ot);
void FILE_OT_filename(struct wmOperatorType *ot);
void FILE_OT_previous(struct wmOperatorType *ot);
void FILE_OT_next(struct wmOperatorType *ot);
void FILE_OT_refresh(struct wmOperatorType *ot);
void FILE_OT_bookmark_toggle(struct wmOperatorType *ot);
void FILE_OT_filenum(struct wmOperatorType *ot);
void FILE_OT_delete(struct wmOperatorType *ot);
void FILE_OT_rename(struct wmOperatorType *ot);

int file_exec(bContext *C, struct wmOperator *exec_op);
int file_cancel_exec(bContext *C, struct wmOperator *unused);
int file_parent_exec(bContext *C, struct wmOperator *unused);
int file_previous_exec(bContext *C, struct wmOperator *unused);
int file_next_exec(bContext *C, struct wmOperator *unused);
int file_filename_exec(bContext *C, struct wmOperator *unused);
int file_directory_exec(bContext *C, struct wmOperator *unused);
int file_directory_new_exec(bContext *C,struct wmOperator *unused);
int file_delete_exec(bContext *C, struct wmOperator *unused);

int file_hilight_set(struct SpaceFile *sfile, struct ARegion *ar, int mx, int my);


/* filesel.c */
float file_string_width(const char* str);
float file_font_pointsize();
void file_change_dir(bContext *C, int checkdir);
int file_select_match(struct SpaceFile *sfile, const char *pattern);
void autocomplete_directory(struct bContext *C, char *str, void *arg_v);

/* file_panels.c */
void file_panels_register(struct ARegionType *art);

#endif /* ED_FILE_INTERN_H */

