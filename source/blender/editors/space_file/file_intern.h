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

/** \file blender/editors/space_file/file_intern.h
 *  \ingroup spfile
 */

#ifndef __FILE_INTERN_H__
#define __FILE_INTERN_H__

/* internal exports only */

struct ARegion;
struct ARegionType;
struct SpaceFile;

/* file_ops.c */
struct ARegion *file_tools_region(struct ScrArea *sa);

/* file_draw.c */
#define TILE_BORDER_X (UI_UNIT_X / 4)
#define TILE_BORDER_Y (UI_UNIT_Y / 4)

/* ui geometry */
#define IMASEL_BUTTONS_HEIGHT (UI_UNIT_Y * 2)
#define IMASEL_BUTTONS_MARGIN (UI_UNIT_Y / 6)

#define SMALL_SIZE_CHECK(_size) ((_size) < 64)  /* Related to FileSelectParams.thumbnail_size. */

void file_draw_buttons(const bContext *C, ARegion *ar);
void file_calc_previews(const bContext *C, ARegion *ar);
void file_draw_list(const bContext *C, ARegion *ar);

void file_draw_check(bContext *C);
void file_draw_check_cb(bContext *C, void *arg1, void *arg2);
bool file_draw_check_exists(SpaceFile *sfile);

/* file_ops.h */
struct wmOperatorType;
struct wmOperator;

typedef enum WalkSelectDirection {
	FILE_SELECT_WALK_UP,
	FILE_SELECT_WALK_DOWN,
	FILE_SELECT_WALK_LEFT,
	FILE_SELECT_WALK_RIGHT,
} WalkSelectDirections;

void FILE_OT_highlight(struct wmOperatorType *ot);
void FILE_OT_select(struct wmOperatorType *ot);
void FILE_OT_select_walk(struct wmOperatorType *ot);
void FILE_OT_select_all(struct wmOperatorType *ot);
void FILE_OT_select_border(struct wmOperatorType *ot);
void FILE_OT_select_bookmark(struct wmOperatorType *ot);
void FILE_OT_bookmark_add(struct wmOperatorType *ot);
void FILE_OT_bookmark_delete(struct wmOperatorType *ot);
void FILE_OT_bookmark_cleanup(struct wmOperatorType *ot);
void FILE_OT_bookmark_move(struct wmOperatorType *ot);
void FILE_OT_reset_recent(wmOperatorType *ot);
void FILE_OT_hidedot(struct wmOperatorType *ot);
void FILE_OT_execute(struct wmOperatorType *ot);
void FILE_OT_cancel(struct wmOperatorType *ot);
void FILE_OT_parent(struct wmOperatorType *ot);
void FILE_OT_directory_new(struct wmOperatorType *ot);
void FILE_OT_previous(struct wmOperatorType *ot);
void FILE_OT_next(struct wmOperatorType *ot);
void FILE_OT_refresh(struct wmOperatorType *ot);
void FILE_OT_bookmark_toggle(struct wmOperatorType *ot);
void FILE_OT_filenum(struct wmOperatorType *ot);
void FILE_OT_delete(struct wmOperatorType *ot);
void FILE_OT_rename(struct wmOperatorType *ot);
void FILE_OT_smoothscroll(struct wmOperatorType *ot);
void FILE_OT_filepath_drop(struct wmOperatorType *ot);

int file_exec(bContext *C, struct wmOperator *exec_op);
int file_cancel_exec(bContext *C, struct wmOperator *unused);
int file_parent_exec(bContext *C, struct wmOperator *unused);
int file_previous_exec(bContext *C, struct wmOperator *unused);
int file_next_exec(bContext *C, struct wmOperator *unused);
int file_directory_new_exec(bContext *C, struct wmOperator *unused);
int file_delete_exec(bContext *C, struct wmOperator *unused);

void file_directory_enter_handle(bContext *C, void *arg_unused, void *arg_but);
void file_filename_enter_handle(bContext *C, void *arg_unused, void *arg_but);

int file_highlight_set(struct SpaceFile *sfile, struct ARegion *ar, int mx, int my);

void file_sfile_filepath_set(struct SpaceFile *sfile, const char *filepath);
void file_sfile_to_operator_ex(bContext *C, struct wmOperator *op, struct SpaceFile *sfile, char *filepath);
void file_sfile_to_operator(bContext *C, struct wmOperator *op, struct SpaceFile *sfile);
void file_operator_to_sfile(bContext *C, struct SpaceFile *sfile, struct wmOperator *op);


/* filesel.c */
void fileselect_file_set(SpaceFile *sfile, const int index);
float file_string_width(const char *str);

float file_font_pointsize(void);
int file_select_match(struct SpaceFile *sfile, const char *pattern, char *matched_file);
int autocomplete_directory(struct bContext *C, char *str, void *arg_v);
int autocomplete_file(struct bContext *C, char *str, void *arg_v);

/* file_panels.c */
void file_panels_register(struct ARegionType *art);

/* file_utils.c */
void file_tile_boundbox(const ARegion *ar, FileLayout *layout, const int file, rcti *r_bounds);

#endif /* __FILE_INTERN_H__ */
