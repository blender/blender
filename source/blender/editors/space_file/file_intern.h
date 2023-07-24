/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup spfile
 */

#pragma once

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* internal exports only */

struct ARegion;
struct ARegionType;
struct AssetLibrary;
struct FileAssetSelectParams;
struct FileSelectParams;
struct SpaceFile;
struct View2D;
struct uiLayout;

/* file_draw.c */

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

void file_draw_check_ex(bContext *C, struct ScrArea *area);
void file_draw_check(bContext *C);
/**
 * For use with; #UI_block_func_set.
 */
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

void FILE_OT_external_operation(struct wmOperatorType *ot);
void file_external_operations_menu_register(void);

/**
 * Variation of #FILE_OT_execute that accounts for some mouse specific handling.
 * Otherwise calls the same logic.
 */
void FILE_OT_mouse_execute(struct wmOperatorType *ot);
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
void FILE_OT_edit_directory_path(struct wmOperatorType *ot);
void FILE_OT_view_selected(struct wmOperatorType *ot);

void file_directory_enter_handle(bContext *C, void *arg_unused, void *arg_but);
void file_filename_enter_handle(bContext *C, void *arg_unused, void *arg_but);

int file_highlight_set(struct SpaceFile *sfile, struct ARegion *region, int mx, int my);

/**
 * Use to set the file selector path from some arbitrary source.
 */
void file_sfile_filepath_set(struct SpaceFile *sfile, const char *filepath);
void file_sfile_to_operator_ex(struct bContext *C,
                               struct Main *bmain,
                               struct wmOperator *op,
                               struct SpaceFile *sfile,
                               char *filepath);
void file_sfile_to_operator(struct bContext *C,
                            struct Main *bmain,
                            struct wmOperator *op,
                            struct SpaceFile *sfile);

void file_operator_to_sfile(struct Main *bmain, struct SpaceFile *sfile, struct wmOperator *op);

/* space_file.c */

extern const char *file_context_dir[]; /* doc access */

/* filesel.c */

void fileselect_refresh_params(struct SpaceFile *sfile);
/**
 * Sets #FileSelectParams.file (name of selected file)
 */
void fileselect_file_set(struct bContext *C, SpaceFile *sfile, int index);
bool file_attribute_column_type_enabled(const FileSelectParams *params,
                                        FileAttributeColumnType column);
/**
 * Check if the region coordinate defined by \a x and \a y are inside the column header.
 */
bool file_attribute_column_header_is_inside(const struct View2D *v2d,
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
int file_select_match(struct SpaceFile *sfile, const char *pattern, char *matched_file);
int autocomplete_directory(struct bContext *C, char *str, void *arg_v);
int autocomplete_file(struct bContext *C, char *str, void *arg_v);

void file_params_smoothscroll_timer_clear(struct wmWindowManager *wm,
                                          struct wmWindow *win,
                                          SpaceFile *sfile);
void file_params_renamefile_clear(struct FileSelectParams *params);
/**
 * Set the renaming-state to #FILE_PARAMS_RENAME_POSTSCROLL_PENDING and trigger the smooth-scroll
 * timer. To be used right after a file was renamed.
 * Note that the caller is responsible for setting the correct rename-file info
 * (#FileSelectParams.renamefile or #FileSelectParams.rename_id).
 */
void file_params_invoke_rename_postscroll(struct wmWindowManager *wm,
                                          struct wmWindow *win,
                                          SpaceFile *sfile);
/**
 * To be executed whenever renaming ends (successfully or not).
 */
void file_params_rename_end(struct wmWindowManager *wm,
                            struct wmWindow *win,
                            SpaceFile *sfile,
                            struct FileDirEntry *rename_file);
/**
 * Helper used by both main update code, and smooth-scroll timer,
 * to try to enable rename editing from #FileSelectParams.renamefile name.
 */
void file_params_renamefile_activate(struct SpaceFile *sfile, struct FileSelectParams *params);

typedef void *onReloadFnData;
typedef void (*onReloadFn)(struct SpaceFile *space_data, onReloadFnData custom_data);
typedef struct SpaceFile_Runtime {
  /* Called once after the file browser has reloaded. Reset to NULL after calling.
   * Use file_on_reload_callback_register() to register a callback. */
  onReloadFn on_reload;
  onReloadFnData on_reload_custom_data;

  /* Indicates, if the current filepath is a blendfile library one, if its status has been checked,
   * and if it is readable. */
  bool is_blendfile_status_set;
  bool is_blendfile_readable;
  ReportList is_blendfile_readable_reports;
} SpaceFile_Runtime;

/**
 * Register an on-reload callback function. Note that there can only be one such function at a
 * time; registering a new one will overwrite the previous one.
 */
void file_on_reload_callback_register(struct SpaceFile *sfile,
                                      onReloadFn callback,
                                      onReloadFnData custom_data);

/* folder_history.cc */

/* not listbase itself */
void folderlist_free(struct ListBase *folderlist);
void folderlist_popdir(struct ListBase *folderlist, char *dir);
void folderlist_pushdir(struct ListBase *folderlist, const char *dir);
const char *folderlist_peeklastdir(struct ListBase *folderlist);
bool folderlist_clear_next(struct SpaceFile *sfile);

void folder_history_list_ensure_for_active_browse_mode(struct SpaceFile *sfile);
void folder_history_list_free(struct SpaceFile *sfile);
struct ListBase folder_history_list_duplicate(struct ListBase *listbase);

/* file_panels.c */

void file_tool_props_region_panels_register(struct ARegionType *art);
void file_execute_region_panels_register(struct ARegionType *art);
void file_tools_region_panels_register(struct ARegionType *art);

/* file_utils.c */

void file_tile_boundbox(const ARegion *region, FileLayout *layout, int file, rcti *r_bounds);

/**
 * If \a path leads to a .blend, remove the trailing slash (if needed).
 */
void file_path_to_ui_path(const char *path, char *r_pathi, int max_size);

/* asset_catalog_tree_view.cc */

/* C-handle for #ed::asset_browser::AssetCatalogFilterSettings. */
typedef struct FileAssetCatalogFilterSettingsHandle FileAssetCatalogFilterSettingsHandle;

void file_create_asset_catalog_tree_view_in_layout(struct AssetLibrary *asset_library,
                                                   struct uiLayout *layout,
                                                   SpaceFile *space_file,
                                                   FileAssetSelectParams *params);

#ifdef __cplusplus

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

#endif

#ifdef __cplusplus
}
#endif
