/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include <string>

#include "BLI_vector.hh"

#include "DNA_uuid_types.h"

struct ARegion;
struct FileAssetSelectParams;
struct FileDirEntry;
struct FileSelectParams;
struct FSMenu;
struct FSMenuEntry;
struct ID;
struct ScrArea;
struct SpaceFile;
struct bContext;
struct bScreen;
struct uiBlock;
struct wmOperator;
struct wmWindow;
struct wmWindowManager;
struct View2D;
struct rcti;
namespace blender::asset_system {
class AssetLibrary;
}

#define FILE_LAYOUT_HOR 1
#define FILE_LAYOUT_VER 2

enum FileAttributeColumnType {
  COLUMN_NONE = -1,
  COLUMN_NAME = 0,
  COLUMN_DATETIME,
  COLUMN_SIZE,

  ATTRIBUTE_COLUMN_MAX
};

struct FileAttributeColumn {
  /** UI name for this column */
  const char *name;

  float width;
  /** The sort type to use when sorting by this column. */
  int sort_type; /* eFileSortType */

  /** Alignment of column texts, header text is always left aligned */
  int text_align; /* eFontStyle_Align */
};

struct FileLayout {
  /* view settings - XXX: move into its own struct. */
  int offset_top;
  /** Height of the header for the different FileAttributeColumn's. */
  int attribute_column_header_h;
  int prv_w;
  int prv_h;
  /** Extra padding to add above any files. Used for horizontal and column list views. */
  int list_padding_top;
  /** Width to draw the file's "tile" (matches the highlight background) with. `tile_border_x` will
   * be added before and after it as padding around the tile. */
  int tile_w;
  /** Height to draw the file's "tile" (matches the highlight background) with. `tile_border_y`
   * will be added above and below it as padding around the tile. */
  int tile_h;
  int tile_border_x;
  int tile_border_y;
  int prv_border_x;
  int prv_border_y;
  int rows;
  /**
   * Those are the major layout columns the files are distributed across,
   * not to be confused with `attribute_columns` array below.
   */
  int flow_columns;
  int width;
  int height;
  int flag;
  int dirty;
  int text_line_height;
  int text_lines_count;
  /**
   * The columns for each item (name, modification date/time, size).
   * Not to be confused with the `flow_columns` above.
   */
  FileAttributeColumn attribute_columns[ATTRIBUTE_COLUMN_MAX];

  /** When we change display size, we may have to update static strings like size of files. */
  short curr_size;
};

struct FileSelection {
  int first;
  int last;
};

/**
 * If needed, create and return the file select parameters for the active browse mode.
 */
FileSelectParams *ED_fileselect_ensure_active_params(SpaceFile *sfile);
/**
 * Get the file select parameters for the active browse mode.
 */
FileSelectParams *ED_fileselect_get_active_params(const SpaceFile *sfile);
FileSelectParams *ED_fileselect_get_file_params(const SpaceFile *sfile);
FileAssetSelectParams *ED_fileselect_get_asset_params(const SpaceFile *sfile);
bool ED_fileselect_is_local_asset_library(const SpaceFile *sfile);

void ED_fileselect_set_params_from_userdef(SpaceFile *sfile);
/**
 * Update the user-preference data for the file space. In fact, this also contains some
 * non-FileSelectParams data, but we can safely ignore this.
 *
 * \param temp_win_size: If the browser was opened in a temporary window,
 * pass its size here so we can store that in the preferences. Otherwise NULL.
 */
void ED_fileselect_params_to_userdef(SpaceFile *sfile);

void ED_fileselect_init_layout(SpaceFile *sfile, ARegion *region);

FileLayout *ED_fileselect_get_layout(SpaceFile *sfile, ARegion *region);

int ED_fileselect_layout_numfiles(FileLayout *layout, ARegion *region);
int ED_fileselect_layout_offset(FileLayout *layout, int x, int y);
FileSelection ED_fileselect_layout_offset_rect(FileLayout *layout, const rcti *rect);

/**
 * Get the currently visible bounds of the layout in screen space. Matches View2D.mask minus the
 * top column-header row.
 */
void ED_fileselect_layout_maskrect(const FileLayout *layout, const View2D *v2d, rcti *r_rect);
bool ED_fileselect_layout_is_inside_pt(const FileLayout *layout, const View2D *v2d, int x, int y);
bool ED_fileselect_layout_isect_rect(const FileLayout *layout,
                                     const View2D *v2d,
                                     const rcti *rect,
                                     rcti *r_dst);
void ED_fileselect_layout_tilepos(const FileLayout *layout, int tile, int *x, int *y);

void ED_operatormacros_file();

void ED_fileselect_clear(wmWindowManager *wm, SpaceFile *sfile);
void ED_fileselect_clear_main_assets(wmWindowManager *wm, SpaceFile *sfile);

void ED_fileselect_exit(wmWindowManager *wm, SpaceFile *sfile);

bool ED_fileselect_is_file_browser(const SpaceFile *sfile);
bool ED_fileselect_is_asset_browser(const SpaceFile *sfile);
blender::asset_system::AssetLibrary *ED_fileselect_active_asset_library_get(
    const SpaceFile *sfile);
ID *ED_fileselect_active_asset_get(const SpaceFile *sfile);

void ED_fileselect_activate_asset_catalog(const SpaceFile *sfile, bUUID catalog_id);

/**
 * Resolve this space's #eFileAssetImportMethod to the #eAssetImportMethod (note the different
 * type) to be used for the actual import of a specific asset.
 * - If the asset system dictates a certain import method, this will be returned.
 * - If the Asset Browser is set to follow the Preferences (#FILE_ASSET_IMPORT_FOLLOW_PREFS), the
 *   asset system determines the import method (which is the default from the Preferences). -1 is
 *   returned if the asset system doesn't specify a method (e.g. because the asset library doesn't
 *   come from the Preferences).
 * - Otherwise, the Asset Browser determines (possibly overrides) the import method.
 *
 * \return -1 on error, for example when #FILE_ASSET_IMPORT_FOLLOW_PREFS was requested but the
 *         active asset library reference couldn't be found in the preferences.
 */
int /* #eAssetImportMethod */ ED_fileselect_asset_import_method_get(const SpaceFile *sfile,
                                                                    const FileDirEntry *file);

/**
 * Activate and select the file that corresponds to the given ID.
 * Pass deferred=true to wait for the next refresh before activating.
 */
void ED_fileselect_activate_by_id(SpaceFile *sfile, ID *asset_id, bool deferred);

void ED_fileselect_deselect_all(SpaceFile *sfile);
void ED_fileselect_activate_by_relpath(SpaceFile *sfile, const char *relative_path);

void ED_fileselect_window_params_get(const wmWindow *win, int r_win_size[2], bool *r_is_maximized);

/**
 * Return the File Browser area in which \a file_operator is active.
 */
ScrArea *ED_fileselect_handler_area_find(const wmWindow *win, const wmOperator *file_operator);
/**
 * Check if there is any area in \a win that acts as a modal File Browser (#SpaceFile.op is set)
 * and return it.
 */
ScrArea *ED_fileselect_handler_area_find_any_with_op(const wmWindow *win);

/**
 * If filepath property is not set on the operator, sets it to
 * the blend file path (or untitled if file is not saved yet) with the given extension.
 */
void ED_fileselect_ensure_default_filepath(bContext *C, wmOperator *op, const char *extension);

blender::Vector<std::string> ED_fileselect_selected_files_full_paths(const SpaceFile *sfile);

/* TODO: Maybe we should move this to BLI?
 * On the other hand, it's using defines from space-file area, so not sure... */
int ED_path_extension_type(const char *path);
int ED_file_extension_icon(const char *path);
int ED_file_icon(const FileDirEntry *file);

void ED_file_read_bookmarks();

/**
 * Support updating the directory even when this isn't the active space
 * needed so RNA properties update function isn't context sensitive, see #70255.
 */
void ED_file_change_dir_ex(bContext *C, ScrArea *area);
void ED_file_change_dir(bContext *C);

void ED_file_path_button(bScreen *screen,
                         const SpaceFile *sfile,
                         FileSelectParams *params,
                         uiBlock *block);

/* File menu stuff */

/* FSMenuEntry's without paths indicate separators */
struct FSMenuEntry {
  FSMenuEntry *next;

  char *path;
  char name[/*FILE_MAXFILE*/ 256];
  short save;
  int icon;
};

enum FSMenuCategory {
  FS_CATEGORY_SYSTEM,
  FS_CATEGORY_SYSTEM_BOOKMARKS,
  FS_CATEGORY_BOOKMARKS,
  FS_CATEGORY_RECENT,
  /** For internal use, a list of known paths that are used to match paths to icons and names. */
  FS_CATEGORY_OTHER,
};

enum FSMenuInsert {
  FS_INSERT_SORTED = (1 << 0),
  FS_INSERT_SAVE = (1 << 1),
  /** moves the item to the front of the list when its not already there */
  FS_INSERT_FIRST = (1 << 2),
  /** just append to preserve delivered order */
  FS_INSERT_LAST = (1 << 3),
};

FSMenu *ED_fsmenu_get();
FSMenuEntry *ED_fsmenu_get_category(FSMenu *fsmenu, FSMenuCategory category);
void ED_fsmenu_set_category(FSMenu *fsmenu, FSMenuCategory category, FSMenuEntry *fsm_head);

int ED_fsmenu_get_nentries(FSMenu *fsmenu, FSMenuCategory category);

FSMenuEntry *ED_fsmenu_get_entry(FSMenu *fsmenu, FSMenuCategory category, int idx);

char *ED_fsmenu_entry_get_path(FSMenuEntry *fsentry);
void ED_fsmenu_entry_set_path(FSMenuEntry *fsentry, const char *path);

char *ED_fsmenu_entry_get_name(FSMenuEntry *fsentry);
void ED_fsmenu_entry_set_name(FSMenuEntry *fsentry, const char *name);

int ED_fsmenu_entry_get_icon(FSMenuEntry *fsentry);
void ED_fsmenu_entry_set_icon(FSMenuEntry *fsentry, int icon);
