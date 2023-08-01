/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#pragma once

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

/* internal exports only */

struct ARegion;
struct ARegionType;
struct AssetLibrary;
struct bContextDataResult;
struct FileAssetSelectParams;
struct FileSelectParams;
struct Main;
struct SpaceFile;
struct View2D;
struct uiLayout;

bool file_main_region_needs_refresh_before_draw(SpaceFile *sfile);

/* `file_context.cc` */

int /*eContextResult*/ file_context(const bContext *C,
                                    const char *member,
                                    bContextDataResult *result);

/* `file_draw.cc` */

#define ATTRIBUTE_COLUMN_PADDING (0.5f * UI_UNIT_X)

/** Related to #FileSelectParams.thumbnail_size. */
#define SMALL_SIZE_CHECK(_size) ((_size) < 64)

void file_calc_previews(const bContext *C, ARegion *region);
void file_draw_list(const bContext *C, ARegion *region);
/**
 * Draw a string hint if the file list is invalid.
 * \return true if the list is invalid and a hint was drawn.
 */
bool file_draw_hint_if_invalid(const bContext *C, const SpaceFile *sfile, ARegion *region);

void file_draw_check_ex(bContext *C, ScrArea *area);
void file_draw_check(bContext *C);
/**
 * For use with; #UI_block_func_set.
 */
void file_draw_check_cb(bContext *C, void *arg1, void *arg2);
bool file_draw_check_exists(SpaceFile *sfile);

/* file_ops.h */

struct wmOperator;
struct wmOperatorType;

void FILE_OT_highlight(wmOperatorType *ot);
void FILE_OT_sort_column_ui_context(wmOperatorType *ot);
void FILE_OT_select(wmOperatorType *ot);
void FILE_OT_select_walk(wmOperatorType *ot);
void FILE_OT_select_all(wmOperatorType *ot);
void FILE_OT_select_box(wmOperatorType *ot);
void FILE_OT_select_bookmark(wmOperatorType *ot);
void FILE_OT_bookmark_add(wmOperatorType *ot);
void FILE_OT_bookmark_delete(wmOperatorType *ot);
void FILE_OT_bookmark_cleanup(wmOperatorType *ot);
void FILE_OT_bookmark_move(wmOperatorType *ot);
void FILE_OT_reset_recent(wmOperatorType *ot);
void FILE_OT_hidedot(wmOperatorType *ot);
void FILE_OT_execute(wmOperatorType *ot);

void FILE_OT_external_operation(wmOperatorType *ot);
void file_external_operations_menu_register(void);

/**
 * Variation of #FILE_OT_execute that accounts for some mouse specific handling.
 * Otherwise calls the same logic.
 */
void FILE_OT_mouse_execute(wmOperatorType *ot);
void FILE_OT_cancel(wmOperatorType *ot);
void FILE_OT_parent(wmOperatorType *ot);
void FILE_OT_directory_new(wmOperatorType *ot);
void FILE_OT_previous(wmOperatorType *ot);
void FILE_OT_next(wmOperatorType *ot);
void FILE_OT_refresh(wmOperatorType *ot);
void FILE_OT_filenum(wmOperatorType *ot);
void FILE_OT_delete(wmOperatorType *ot);
void FILE_OT_rename(wmOperatorType *ot);
void FILE_OT_smoothscroll(wmOperatorType *ot);
void FILE_OT_filepath_drop(wmOperatorType *ot);
void FILE_OT_start_filter(wmOperatorType *ot);
void FILE_OT_edit_directory_path(wmOperatorType *ot);
void FILE_OT_view_selected(wmOperatorType *ot);

void file_directory_enter_handle(bContext *C, void *arg_unused, void *arg_but);
void file_filename_enter_handle(bContext *C, void *arg_unused, void *arg_but);

int file_highlight_set(SpaceFile *sfile, ARegion *region, int mx, int my);

/**
 * Use to set the file selector path from some arbitrary source.
 */
void file_sfile_filepath_set(SpaceFile *sfile, const char *filepath);
void file_sfile_to_operator_ex(
    bContext *C, Main *bmain, wmOperator *op, SpaceFile *sfile, char *filepath);
void file_sfile_to_operator(bContext *C, Main *bmain, wmOperator *op, SpaceFile *sfile);

void file_operator_to_sfile(Main *bmain, SpaceFile *sfile, wmOperator *op);

/* `space_file.cc` */

extern "C" const char *file_context_dir[]; /* doc access */

/* `filesel.cc` */

void fileselect_refresh_params(SpaceFile *sfile);
/**
 * Sets #FileSelectParams.file (name of selected file)
 */
void fileselect_file_set(bContext *C, SpaceFile *sfile, int index);
bool file_attribute_column_type_enabled(const FileSelectParams *params,
                                        FileAttributeColumnType column);
/**
 * Check if the region coordinate defined by \a x and \a y are inside the column header.
 */
bool file_attribute_column_header_is_inside(const View2D *v2d,
                                            const FileLayout *layout,
                                            int x,
                                            int y);
/**
 * Find the column type at region coordinate given by \a x (y doesn't matter for this).
 */
FileAttributeColumnType file_attribute_column_type_find_isect(const View2D *v2d,
                                                              const FileSelectParams *params,
                                                              FileLayout *layout,
                                                              int x);
float file_string_width(const char *str);

float file_font_pointsize(void);
void file_select_deselect_all(SpaceFile *sfile, eDirEntry_SelectFlag flag);
int file_select_match(SpaceFile *sfile, const char *pattern, char *matched_file);
int autocomplete_directory(bContext *C, char *str, void *arg_v);
int autocomplete_file(bContext *C, char *str, void *arg_v);

void file_params_smoothscroll_timer_clear(wmWindowManager *wm, wmWindow *win, SpaceFile *sfile);
void file_params_renamefile_clear(FileSelectParams *params);
/**
 * Set the renaming-state to #FILE_PARAMS_RENAME_POSTSCROLL_PENDING and trigger the smooth-scroll
 * timer. To be used right after a file was renamed.
 * Note that the caller is responsible for setting the correct rename-file info
 * (#FileSelectParams.renamefile or #FileSelectParams.rename_id).
 */
void file_params_invoke_rename_postscroll(wmWindowManager *wm, wmWindow *win, SpaceFile *sfile);
/**
 * To be executed whenever renaming ends (successfully or not).
 */
void file_params_rename_end(wmWindowManager *wm,
                            wmWindow *win,
                            SpaceFile *sfile,
                            FileDirEntry *rename_file);
/**
 * Helper used by both main update code, and smooth-scroll timer,
 * to try to enable rename editing from #FileSelectParams.renamefile name.
 */
void file_params_renamefile_activate(SpaceFile *sfile, FileSelectParams *params);

typedef void *onReloadFnData;
typedef void (*onReloadFn)(SpaceFile *space_data, onReloadFnData custom_data);
struct SpaceFile_Runtime {
  /* Called once after the file browser has reloaded. Reset to NULL after calling.
   * Use file_on_reload_callback_register() to register a callback. */
  onReloadFn on_reload;
  onReloadFnData on_reload_custom_data;

  /* Indicates, if the current filepath is a blendfile library one, if its status has been checked,
   * and if it is readable. */
  bool is_blendfile_status_set;
  bool is_blendfile_readable;
  ReportList is_blendfile_readable_reports;
};

/**
 * Register an on-reload callback function. Note that there can only be one such function at a
 * time; registering a new one will overwrite the previous one.
 */
void file_on_reload_callback_register(SpaceFile *sfile,
                                      onReloadFn callback,
                                      onReloadFnData custom_data);

/* folder_history.cc */

/* not listbase itself */
void folderlist_free(ListBase *folderlist);
void folderlist_popdir(ListBase *folderlist, char *dir);
void folderlist_pushdir(ListBase *folderlist, const char *dir);
const char *folderlist_peeklastdir(ListBase *folderlist);
bool folderlist_clear_next(SpaceFile *sfile);

void folder_history_list_ensure_for_active_browse_mode(SpaceFile *sfile);
void folder_history_list_free(SpaceFile *sfile);
ListBase folder_history_list_duplicate(ListBase *listbase);

/* `file_panels.cc` */

void file_tool_props_region_panels_register(ARegionType *art);
void file_execute_region_panels_register(ARegionType *art);
void file_tools_region_panels_register(ARegionType *art);

/* `file_utils.cc` */

void file_tile_boundbox(const ARegion *region, FileLayout *layout, int file, rcti *r_bounds);

/**
 * If \a path leads to a .blend, remove the trailing slash (if needed).
 */
void file_path_to_ui_path(const char *path, char *r_pathi, int max_size);

/* asset_catalog_tree_view.cc */

/* C-handle for #ed::asset_browser::AssetCatalogFilterSettings. */
typedef struct FileAssetCatalogFilterSettingsHandle FileAssetCatalogFilterSettingsHandle;

void file_create_asset_catalog_tree_view_in_layout(AssetLibrary *asset_library,
                                                   uiLayout *layout,
                                                   SpaceFile *space_file,
                                                   FileAssetSelectParams *params);

namespace blender::asset_system {
class AssetLibrary;
}

FileAssetCatalogFilterSettingsHandle *file_create_asset_catalog_filter_settings(void);
void file_delete_asset_catalog_filter_settings(
    FileAssetCatalogFilterSettingsHandle **filter_settings_handle);
/**
 * \return True if the file list should update its filtered results
 * (e.g. because filtering parameters changed).
 */
bool file_set_asset_catalog_filter_settings(
    FileAssetCatalogFilterSettingsHandle *filter_settings_handle,
    eFileSel_Params_AssetCatalogVisibility catalog_visibility,
    bUUID catalog_id);
void file_ensure_updated_catalog_filter_data(
    FileAssetCatalogFilterSettingsHandle *filter_settings_handle,
    const blender::asset_system::AssetLibrary *asset_library);
bool file_is_asset_visible_in_catalog_filter_settings(
    const FileAssetCatalogFilterSettingsHandle *filter_settings_handle,
    const AssetMetaData *asset_data);
