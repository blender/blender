/*
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
 */

/** \file
 * \ingroup spfile
 */

#pragma once

/* internal exports only */

struct ARegion;
struct ARegionType;
struct FileSelectParams;
struct SpaceFile;
struct View2D;

/* file_draw.c */
#define ATTRIBUTE_COLUMN_PADDING (0.5f * UI_UNIT_X)

#define SMALL_SIZE_CHECK(_size) ((_size) < 64) /* Related to FileSelectParams.thumbnail_size. */

void file_calc_previews(const bContext *C, ARegion *region);
void file_draw_list(const bContext *C, ARegion *region);
bool file_draw_hint_if_invalid(const SpaceFile *sfile, const ARegion *region);

void file_draw_check_ex(bContext *C, struct ScrArea *area);
void file_draw_check(bContext *C);
void file_draw_check_cb(bContext *C, void *arg1, void *arg2);
bool file_draw_check_exists(SpaceFile *sfile);

/* file_ops.h */
struct wmOperator;
struct wmOperatorType;

void FILE_OT_highlight(struct wmOperatorType *ot);
void FILE_OT_sort_column_ui_context(struct wmOperatorType *ot);
void FILE_OT_select(struct wmOperatorType *ot);
void FILE_OT_select_walk(struct wmOperatorType *ot);
void FILE_OT_select_all(struct wmOperatorType *ot);
void FILE_OT_select_box(struct wmOperatorType *ot);
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
void FILE_OT_filenum(struct wmOperatorType *ot);
void FILE_OT_delete(struct wmOperatorType *ot);
void FILE_OT_rename(struct wmOperatorType *ot);
void FILE_OT_smoothscroll(struct wmOperatorType *ot);
void FILE_OT_filepath_drop(struct wmOperatorType *ot);
void FILE_OT_start_filter(struct wmOperatorType *ot);
void FILE_OT_view_selected(struct wmOperatorType *ot);

void file_directory_enter_handle(bContext *C, void *arg_unused, void *arg_but);
void file_filename_enter_handle(bContext *C, void *arg_unused, void *arg_but);

int file_highlight_set(struct SpaceFile *sfile, struct ARegion *region, int mx, int my);

void file_sfile_filepath_set(struct SpaceFile *sfile, const char *filepath);
void file_sfile_to_operator_ex(struct Main *bmain,
                               struct wmOperator *op,
                               struct SpaceFile *sfile,
                               char *filepath);
void file_sfile_to_operator(struct Main *bmain, struct wmOperator *op, struct SpaceFile *sfile);

void file_operator_to_sfile(struct Main *bmain, struct SpaceFile *sfile, struct wmOperator *op);

/* filesel.c */
void fileselect_refresh_params(struct SpaceFile *sfile);
void fileselect_file_set(SpaceFile *sfile, const int index);
bool file_attribute_column_type_enabled(const FileSelectParams *params,
                                        FileAttributeColumnType column);
bool file_attribute_column_header_is_inside(const struct View2D *v2d,
                                            const FileLayout *layout,
                                            int x,
                                            int y);
FileAttributeColumnType file_attribute_column_type_find_isect(const View2D *v2d,
                                                              const FileSelectParams *params,
                                                              FileLayout *layout,
                                                              int x);
float file_string_width(const char *str);

float file_font_pointsize(void);
int file_select_match(struct SpaceFile *sfile, const char *pattern, char *matched_file);
int autocomplete_directory(struct bContext *C, char *str, void *arg_v);
int autocomplete_file(struct bContext *C, char *str, void *arg_v);

void file_params_renamefile_activate(struct SpaceFile *sfile, struct FileSelectParams *params);

typedef void *onReloadFnData;
typedef void (*onReloadFn)(struct SpaceFile *space_data, onReloadFnData custom_data);
typedef struct SpaceFile_Runtime {
  /* Called once after the file browser has reloaded. Reset to NULL after calling.
   * Use file_on_reload_callback_register() to register a callback. */
  onReloadFn on_reload;
  onReloadFnData on_reload_custom_data;
} SpaceFile_Runtime;

/* Register an on-reload callback function. Note that there can only be one such function at a
 * time; registering a new one will overwrite the previous one. */
void file_on_reload_callback_register(struct SpaceFile *sfile,
                                      onReloadFn callback,
                                      onReloadFnData custom_data);

/* file_panels.c */
void file_tool_props_region_panels_register(struct ARegionType *art);
void file_execute_region_panels_register(struct ARegionType *art);

/* file_utils.c */
void file_tile_boundbox(const ARegion *region, FileLayout *layout, const int file, rcti *r_bounds);

void file_path_to_ui_path(const char *path, char *r_pathi, int max_size);
