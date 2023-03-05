/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_uuid_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct FileAssetSelectParams;
struct FileDirEntry;
struct FileSelectParams;
struct ScrArea;
struct SpaceFile;
struct bContext;
struct bScreen;
struct uiBlock;
struct wmOperator;
struct wmWindow;
struct wmWindowManager;

#define FILE_LAYOUT_HOR 1
#define FILE_LAYOUT_VER 2

typedef enum FileAttributeColumnType {
  COLUMN_NONE = -1,
  COLUMN_NAME = 0,
  COLUMN_DATETIME,
  COLUMN_SIZE,

  ATTRIBUTE_COLUMN_MAX
} FileAttributeColumnType;

typedef struct FileAttributeColumn {
  /** UI name for this column */
  const char *name;

  float width;
  /* The sort type to use when sorting by this column. */
  int sort_type; /* eFileSortType */

  /* Alignment of column texts, header text is always left aligned */
  int text_align; /* eFontStyle_Align */
} FileAttributeColumn;

typedef struct FileLayout {
  /* view settings - XXX: move into own struct. */
  int offset_top;
  /* Height of the header for the different FileAttributeColumn's. */
  int attribute_column_header_h;
  int prv_w;
  int prv_h;
  int tile_w;
  int tile_h;
  int tile_border_x;
  int tile_border_y;
  int prv_border_x;
  int prv_border_y;
  int rows;
  /* Those are the major layout columns the files are distributed across, not to be confused with
   * 'attribute_columns' array below. */
  int flow_columns;
  int width;
  int height;
  int flag;
  int dirty;
  int textheight;
  /* The columns for each item (name, modification date/time, size). Not to be confused with the
   * 'flow_columns' above. */
  FileAttributeColumn attribute_columns[ATTRIBUTE_COLUMN_MAX];

  /* When we change display size, we may have to update static strings like size of files... */
  short curr_size;
} FileLayout;

typedef struct FileSelection {
  int first;
  int last;
} FileSelection;

struct View2D;
struct rcti;

/**
 * If needed, create and return the file select parameters for the active browse mode.
 */
struct FileSelectParams *ED_fileselect_ensure_active_params(struct SpaceFile *sfile);
/**
 * Get the file select parameters for the active browse mode.
 */
struct FileSelectParams *ED_fileselect_get_active_params(const struct SpaceFile *sfile);
struct FileSelectParams *ED_fileselect_get_file_params(const struct SpaceFile *sfile);
struct FileAssetSelectParams *ED_fileselect_get_asset_params(const struct SpaceFile *sfile);
bool ED_fileselect_is_local_asset_library(const struct SpaceFile *sfile);

void ED_fileselect_set_params_from_userdef(struct SpaceFile *sfile);
/**
 * Update the user-preference data for the file space. In fact, this also contains some
 * non-FileSelectParams data, but we can safely ignore this.
 *
 * \param temp_win_size: If the browser was opened in a temporary window,
 * pass its size here so we can store that in the preferences. Otherwise NULL.
 */
void ED_fileselect_params_to_userdef(struct SpaceFile *sfile,
                                     const int temp_win_size[2],
                                     bool is_maximized);

void ED_fileselect_init_layout(struct SpaceFile *sfile, struct ARegion *region);

FileLayout *ED_fileselect_get_layout(struct SpaceFile *sfile, struct ARegion *region);

int ED_fileselect_layout_numfiles(FileLayout *layout, struct ARegion *region);
int ED_fileselect_layout_offset(FileLayout *layout, int x, int y);
FileSelection ED_fileselect_layout_offset_rect(FileLayout *layout, const struct rcti *rect);

/**
 * Get the currently visible bounds of the layout in screen space. Matches View2D.mask minus the
 * top column-header row.
 */
void ED_fileselect_layout_maskrect(const FileLayout *layout,
                                   const struct View2D *v2d,
                                   struct rcti *r_rect);
bool ED_fileselect_layout_is_inside_pt(const FileLayout *layout,
                                       const struct View2D *v2d,
                                       int x,
                                       int y);
bool ED_fileselect_layout_isect_rect(const FileLayout *layout,
                                     const struct View2D *v2d,
                                     const struct rcti *rect,
                                     struct rcti *r_dst);
void ED_fileselect_layout_tilepos(const FileLayout *layout, int tile, int *x, int *y);

void ED_operatormacros_file(void);

void ED_fileselect_clear(struct wmWindowManager *wm, struct SpaceFile *sfile);

void ED_fileselect_exit(struct wmWindowManager *wm, struct SpaceFile *sfile);

bool ED_fileselect_is_file_browser(const struct SpaceFile *sfile);
bool ED_fileselect_is_asset_browser(const struct SpaceFile *sfile);
struct AssetLibrary *ED_fileselect_active_asset_library_get(const struct SpaceFile *sfile);
struct ID *ED_fileselect_active_asset_get(const struct SpaceFile *sfile);

void ED_fileselect_activate_asset_catalog(const struct SpaceFile *sfile, bUUID catalog_id);

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
int /* #eAssetImportMethod */ ED_fileselect_asset_import_method_get(
    const struct SpaceFile *sfile, const struct FileDirEntry *file);

/**
 * Activate and select the file that corresponds to the given ID.
 * Pass deferred=true to wait for the next refresh before activating.
 */
void ED_fileselect_activate_by_id(struct SpaceFile *sfile, struct ID *asset_id, bool deferred);

void ED_fileselect_deselect_all(struct SpaceFile *sfile);
void ED_fileselect_activate_by_relpath(struct SpaceFile *sfile, const char *relative_path);

void ED_fileselect_window_params_get(const struct wmWindow *win,
                                     int win_size[2],
                                     bool *is_maximized);

/**
 * Return the File Browser area in which \a file_operator is active.
 */
struct ScrArea *ED_fileselect_handler_area_find(const struct wmWindow *win,
                                                const struct wmOperator *file_operator);
/**
 * Check if there is any area in \a win that acts as a modal File Browser (#SpaceFile.op is set)
 * and return it.
 */
struct ScrArea *ED_fileselect_handler_area_find_any_with_op(const struct wmWindow *win);

/**
 * If filepath property is not set on the operator, sets it to
 * the blend file path (or untitled if file is not saved yet) with the given extension.
 */
void ED_fileselect_ensure_default_filepath(struct bContext *C,
                                           struct wmOperator *op,
                                           const char *extension);

/* TODO: Maybe we should move this to BLI?
 * On the other hand, it's using defines from space-file area, so not sure... */
int ED_path_extension_type(const char *path);
int ED_file_extension_icon(const char *path);
int ED_file_icon(const struct FileDirEntry *file);

void ED_file_read_bookmarks(void);

/**
 * Support updating the directory even when this isn't the active space
 * needed so RNA properties update function isn't context sensitive, see #70255.
 */
void ED_file_change_dir_ex(struct bContext *C, struct ScrArea *area);
void ED_file_change_dir(struct bContext *C);

void ED_file_path_button(struct bScreen *screen,
                         const struct SpaceFile *sfile,
                         struct FileSelectParams *params,
                         struct uiBlock *block);

/* File menu stuff */

/* FSMenuEntry's without paths indicate separators */
typedef struct FSMenuEntry {
  struct FSMenuEntry *next;

  char *path;
  char name[256]; /* FILE_MAXFILE */
  short save;
  short valid;
  int icon;
} FSMenuEntry;

typedef enum FSMenuCategory {
  FS_CATEGORY_SYSTEM,
  FS_CATEGORY_SYSTEM_BOOKMARKS,
  FS_CATEGORY_BOOKMARKS,
  FS_CATEGORY_RECENT,
  /* For internal use, a list of known paths that are used to match paths to icons and names. */
  FS_CATEGORY_OTHER,
} FSMenuCategory;

typedef enum FSMenuInsert {
  FS_INSERT_SORTED = (1 << 0),
  FS_INSERT_SAVE = (1 << 1),
  /** moves the item to the front of the list when its not already there */
  FS_INSERT_FIRST = (1 << 2),
  /** just append to preserve delivered order */
  FS_INSERT_LAST = (1 << 3),
  /** Do not validate the link when inserted. */
  FS_INSERT_NO_VALIDATE = (1 << 4),
} FSMenuInsert;

struct FSMenu;
struct FSMenuEntry;

struct FSMenu *ED_fsmenu_get(void);
struct FSMenuEntry *ED_fsmenu_get_category(struct FSMenu *fsmenu, FSMenuCategory category);
void ED_fsmenu_set_category(struct FSMenu *fsmenu,
                            FSMenuCategory category,
                            struct FSMenuEntry *fsm_head);

int ED_fsmenu_get_nentries(struct FSMenu *fsmenu, FSMenuCategory category);

struct FSMenuEntry *ED_fsmenu_get_entry(struct FSMenu *fsmenu, FSMenuCategory category, int idx);

char *ED_fsmenu_entry_get_path(struct FSMenuEntry *fsentry);
void ED_fsmenu_entry_set_path(struct FSMenuEntry *fsentry, const char *path);

char *ED_fsmenu_entry_get_name(struct FSMenuEntry *fsentry);
void ED_fsmenu_entry_set_name(struct FSMenuEntry *fsentry, const char *name);

int ED_fsmenu_entry_get_icon(struct FSMenuEntry *fsentry);
void ED_fsmenu_entry_set_icon(struct FSMenuEntry *fsentry, int icon);

#ifdef __cplusplus
}
#endif
