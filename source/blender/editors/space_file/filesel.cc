/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include <cstring>

#include <sys/stat.h>
#include <sys/types.h>

/* path/file handling stuff */
#ifdef WIN32
#  include "BLI_winstuff.h"
#  include <direct.h>
#  include <io.h>
#else
#  include <dirent.h>
#  include <sys/times.h>
#  include <unistd.h>
#endif

#include "AS_asset_representation.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_fnmatch.h"
#include "BLI_math_base.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_main.hh"
#include "BKE_preferences.h"

#include "BLO_userdef_default.h"

#include "BLF_api.hh"

#include "ED_fileselect.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_view2d.hh"

#include "AS_essentials_library.hh"

#include "file_intern.hh"
#include "filelist.hh"

static void fileselect_initialize_params_common(SpaceFile *sfile, FileSelectParams *params)
{
  const char *blendfile_path = BKE_main_blendfile_path_from_global();

  /* operator has no setting for this */
  params->active_file = -1;

  if (!params->dir[0]) {
    if (blendfile_path[0] != '\0') {
      BLI_path_split_dir_part(blendfile_path, params->dir, sizeof(params->dir));
    }
    else {
      const char *doc_path = BKE_appdir_folder_default();
      if (doc_path) {
        STRNCPY(params->dir, doc_path);
      }
    }
  }

  folder_history_list_ensure_for_active_browse_mode(sfile);
  folderlist_pushdir(sfile->folders_prev, params->dir);

  /* Switching thumbnails needs to recalc layout #28809. */
  if (sfile->layout) {
    sfile->layout->dirty = true;
  }
}

static void fileselect_ensure_updated_asset_params(SpaceFile *sfile)
{
  BLI_assert(sfile->browse_mode == FILE_BROWSE_MODE_ASSETS);
  BLI_assert(sfile->op == nullptr);

  FileAssetSelectParams *asset_params = sfile->asset_params;

  if (!asset_params) {
    asset_params = sfile->asset_params = static_cast<FileAssetSelectParams *>(
        MEM_callocN(sizeof(*asset_params), "FileAssetSelectParams"));
    asset_params->base_params.details_flags = U_default.file_space_data.details_flags;
    asset_params->asset_library_ref.type = ASSET_LIBRARY_ALL;
    asset_params->asset_library_ref.custom_library_index = -1;
    asset_params->import_method = FILE_ASSET_IMPORT_FOLLOW_PREFS;
    asset_params->import_flags = FILE_ASSET_IMPORT_INSTANCE_COLLECTIONS_ON_LINK;
  }

  FileSelectParams *base_params = &asset_params->base_params;
  base_params->file[0] = '\0';
  base_params->filter_glob[0] = '\0';
  base_params->flag |= U_default.file_space_data.flag | FILE_ASSETS_ONLY | FILE_FILTER;
  base_params->flag &= ~FILE_DIRSEL_ONLY;
  base_params->filter |= FILE_TYPE_BLENDERLIB;
  base_params->filter_id = FILTER_ID_ALL;
  base_params->display = FILE_IMGDISPLAY;
  base_params->sort = FILE_SORT_ASSET_CATALOG;
  /* No details columns supported for assets (wouldn't contain anything), disable them all. */
  base_params->details_flags = 0;
  /* Asset libraries include all sub-directories, so enable maximal recursion. */
  base_params->recursion_level = FILE_SELECT_MAX_RECURSIONS;
  /* 'SMALL' size by default. More reasonable since this is typically used as regular editor,
   * space is more of an issue here. */
  base_params->thumbnail_size = 96;
  base_params->list_thumbnail_size = 32;
  base_params->list_column_size = 220;

  fileselect_initialize_params_common(sfile, base_params);
}

/**
 * \note #RNA_struct_property_is_set_ex is used here because we want
 * the previously used settings to be used here rather than overriding them.
 */
static FileSelectParams *fileselect_ensure_updated_file_params(SpaceFile *sfile)
{
  BLI_assert(sfile->browse_mode == FILE_BROWSE_MODE_FILES);

  FileSelectParams *params;
  wmOperator *op = sfile->op;

  const char *blendfile_path = BKE_main_blendfile_path_from_global();

  /* create new parameters if necessary */
  if (!sfile->params) {
    sfile->params = MEM_callocN<FileSelectParams>("fileselparams");
    /* set path to most recently opened .blend */
    BLI_path_split_dir_file(blendfile_path,
                            sfile->params->dir,
                            sizeof(sfile->params->dir),
                            sfile->params->file,
                            sizeof(sfile->params->file));
    sfile->params->filter_glob[0] = '\0';
    sfile->params->thumbnail_size = U_default.file_space_data.thumbnail_size;
    sfile->params->details_flags = U_default.file_space_data.details_flags;
    sfile->params->filter_id = U_default.file_space_data.filter_id;
    sfile->params->list_thumbnail_size = 16;
    sfile->params->list_column_size = 500;
  }

  params = sfile->params;

  /* set the parameters from the operator, if it exists */
  if (op) {
    PropertyRNA *prop;
    const bool is_files = (RNA_struct_find_property(op->ptr, "files") != nullptr);
    const bool is_filepath = (RNA_struct_find_property(op->ptr, "filepath") != nullptr);
    const bool is_filename = (RNA_struct_find_property(op->ptr, "filename") != nullptr);
    const bool is_directory = (RNA_struct_find_property(op->ptr, "directory") != nullptr);
    const bool is_relative_path = (RNA_struct_find_property(op->ptr, "relative_path") != nullptr);

    BLI_strncpy_utf8(
        params->title, WM_operatortype_name(op->type, op->ptr).c_str(), sizeof(params->title));

    if ((prop = RNA_struct_find_property(op->ptr, "filemode"))) {
      params->type = RNA_property_int_get(op->ptr, prop);
    }
    else {
      params->type = FILE_SPECIAL;
    }

    if (is_filepath && RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
      char filepath[FILE_MAX];
      RNA_string_get(op->ptr, "filepath", filepath);
      if (params->type == FILE_LOADLIB) {
        STRNCPY(params->dir, filepath);
        params->file[0] = '\0';
      }
      else {
        BLI_path_split_dir_file(
            filepath, params->dir, sizeof(params->dir), params->file, sizeof(params->file));
      }
    }
    else {
      if (is_directory && RNA_struct_property_is_set_ex(op->ptr, "directory", false)) {
        RNA_string_get(op->ptr, "directory", params->dir);
        params->file[0] = '\0';
      }

      if (is_filename && RNA_struct_property_is_set_ex(op->ptr, "filename", false)) {
        RNA_string_get(op->ptr, "filename", params->file);
      }
    }

    if (params->dir[0]) {
      BLI_path_abs(params->dir, blendfile_path);
      BLI_path_normalize_dir(params->dir, sizeof(params->dir));
    }

    params->flag = 0;
    if (is_directory == true && is_filename == false && is_filepath == false && is_files == false)
    {
      params->flag |= FILE_DIRSEL_ONLY;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "check_existing"))) {
      params->flag |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_CHECK_EXISTING) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "hide_props_region"))) {
      params->flag |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_HIDE_TOOL_PROPS) : 0;
    }

    params->filter = 0;
    if ((prop = RNA_struct_find_property(op->ptr, "filter_blender"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_BLENDER) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_blenlib"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_BLENDERLIB) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_backup"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_BLENDER_BACKUP) :
                                                                  0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_image"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_IMAGE) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_movie"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_MOVIE) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_python"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_PYSCRIPT) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_font"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_FTFONT) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_sound"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_SOUND) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_text"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_TEXT) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_archive"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_ARCHIVE) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_folder"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_FOLDER) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_btx"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_BTX) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_alembic"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_ALEMBIC) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_usd"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_USD) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_obj"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_OBJECT_IO) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_volume"))) {
      params->filter |= RNA_property_boolean_get(op->ptr, prop) ? int(FILE_TYPE_VOLUME) : 0;
    }
    if ((prop = RNA_struct_find_property(op->ptr, "filter_glob"))) {
      /* Protection against Python scripts not setting proper size limit. */
      char *glob = RNA_property_string_get_alloc(op->ptr, prop, nullptr, 0, nullptr);
      BLI_SCOPED_DEFER([&]() { MEM_freeN(glob); });
      STRNCPY_UTF8(params->filter_glob, glob);
      /* Fix stupid things that truncating might have generated,
       * like last group being a 'match everything' wildcard-only one... */
      BLI_path_extension_glob_validate(params->filter_glob);
      if (glob[0] != '\0') {
        params->filter |= (FILE_TYPE_OPERATOR | FILE_TYPE_FOLDER);
      }
    }
    else {
      params->filter_glob[0] = '\0';
    }

    if (params->filter != 0) {
      if (U.uiflag & USER_FILTERFILEEXTS) {
        params->flag |= FILE_FILTER;
      }
      else {
        params->flag &= ~FILE_FILTER;
      }
    }

    if (U.uiflag & USER_HIDE_DOT) {
      params->flag |= FILE_HIDE_DOT;
    }
    else {
      params->flag &= ~FILE_HIDE_DOT;
    }

    if (params->type == FILE_LOADLIB) {
      params->flag |= RNA_boolean_get(op->ptr, "link") ? FILE_LINK : 0;
      params->flag |= RNA_boolean_get(op->ptr, "autoselect") ? FILE_AUTOSELECT : 0;
      params->flag |= RNA_boolean_get(op->ptr, "active_collection") ? FILE_ACTIVE_COLLECTION : 0;
    }

    if ((prop = RNA_struct_find_property(op->ptr, "allow_path_tokens"))) {
      params->flag |= RNA_property_boolean_get(op->ptr, prop) ? FILE_PATH_TOKENS_ALLOW : 0;
    }

    if ((prop = RNA_struct_find_property(op->ptr, "display_type"))) {
      params->display = RNA_property_enum_get(op->ptr, prop);
    }

    if (params->display == FILE_DEFAULTDISPLAY) {
      params->display = U_default.file_space_data.display_type;
    }

    if ((prop = RNA_struct_find_property(op->ptr, "sort_method"))) {
      params->sort = RNA_property_enum_get(op->ptr, prop);
    }

    if (params->sort == FILE_SORT_DEFAULT) {
      params->sort = U_default.file_space_data.sort_type;
    }

    if (is_relative_path) {
      if ((prop = RNA_struct_find_property(op->ptr, "relative_path"))) {
        if (!RNA_property_is_set_ex(op->ptr, prop, false)) {
          RNA_property_boolean_set(op->ptr, prop, (U.flag & USER_RELPATHS) != 0);
        }
      }
    }
  }
  else {
    /* default values, if no operator */
    params->type = FILE_UNIX;
    params->flag |= U_default.file_space_data.flag;
    params->flag &= ~FILE_DIRSEL_ONLY;
    params->display = FILE_VERTICALDISPLAY;
    params->sort = FILE_SORT_ALPHA;
    params->filter = 0;
    params->filter_glob[0] = '\0';
  }

  fileselect_initialize_params_common(sfile, params);

  return params;
}

FileSelectParams *ED_fileselect_ensure_active_params(SpaceFile *sfile)
{
  switch ((eFileBrowse_Mode)sfile->browse_mode) {
    case FILE_BROWSE_MODE_FILES:
      if (!sfile->params) {
        fileselect_ensure_updated_file_params(sfile);
      }
      return sfile->params;
    case FILE_BROWSE_MODE_ASSETS:
      if (!sfile->asset_params) {
        fileselect_ensure_updated_asset_params(sfile);
      }
      return &sfile->asset_params->base_params;
  }

  BLI_assert_msg(0, "Invalid browse mode set in file space.");
  return nullptr;
}

FileSelectParams *ED_fileselect_get_active_params(const SpaceFile *sfile)
{
  if (!sfile) {
    /* Sometimes called in poll before space type was checked. */
    return nullptr;
  }

  switch ((eFileBrowse_Mode)sfile->browse_mode) {
    case FILE_BROWSE_MODE_FILES:
      return sfile->params;
    case FILE_BROWSE_MODE_ASSETS:
      return (FileSelectParams *)sfile->asset_params;
  }

  BLI_assert_msg(0, "Invalid browse mode set in file space.");
  return nullptr;
}

FileSelectParams *ED_fileselect_get_file_params(const SpaceFile *sfile)
{
  return (sfile->browse_mode == FILE_BROWSE_MODE_FILES) ? sfile->params : nullptr;
}

FileAssetSelectParams *ED_fileselect_get_asset_params(const SpaceFile *sfile)
{
  return (sfile->browse_mode == FILE_BROWSE_MODE_ASSETS) ? sfile->asset_params : nullptr;
}

bool ED_fileselect_is_local_asset_library(const SpaceFile *sfile)
{
  const FileAssetSelectParams *asset_params = ED_fileselect_get_asset_params(sfile);
  if (asset_params == nullptr) {
    return false;
  }
  return asset_params->asset_library_ref.type == ASSET_LIBRARY_LOCAL;
}

static void fileselect_refresh_asset_params(FileAssetSelectParams *asset_params)
{
  AssetLibraryReference *library = &asset_params->asset_library_ref;
  FileSelectParams *base_params = &asset_params->base_params;
  bUserAssetLibrary *user_library = nullptr;

  /* Ensure valid repository, or fall-back to local one. */
  if (library->type == ASSET_LIBRARY_CUSTOM) {
    BLI_assert(library->custom_library_index >= 0);

    user_library = BKE_preferences_asset_library_find_index(&U, library->custom_library_index);
    if (!user_library) {
      library->type = ASSET_LIBRARY_ALL;
    }
  }

  switch (eAssetLibraryType(library->type)) {
    case ASSET_LIBRARY_ESSENTIALS:
      STRNCPY(base_params->dir, blender::asset_system::essentials_directory_path().c_str());
      base_params->type = FILE_ASSET_LIBRARY;
      break;
    case ASSET_LIBRARY_ALL:
      base_params->dir[0] = '\0';
      base_params->type = FILE_ASSET_LIBRARY_ALL;
      break;
    case ASSET_LIBRARY_LOCAL:
      base_params->dir[0] = '\0';
      base_params->type = FILE_MAIN_ASSET;
      break;
    case ASSET_LIBRARY_CUSTOM:
      BLI_assert(user_library);
      STRNCPY(base_params->dir, user_library->dirpath);
      BLI_path_slash_native(base_params->dir);
      base_params->type = FILE_ASSET_LIBRARY;
      break;
  }
}

void fileselect_refresh_params(SpaceFile *sfile)
{
  FileAssetSelectParams *asset_params = ED_fileselect_get_asset_params(sfile);
  if (asset_params) {
    fileselect_refresh_asset_params(asset_params);
  }
}

bool ED_fileselect_is_file_browser(const SpaceFile *sfile)
{
  return (sfile->browse_mode == FILE_BROWSE_MODE_FILES);
}

bool ED_fileselect_is_asset_browser(const SpaceFile *sfile)
{
  return (sfile->browse_mode == FILE_BROWSE_MODE_ASSETS);
}

blender::asset_system::AssetLibrary *ED_fileselect_active_asset_library_get(const SpaceFile *sfile)
{
  if (!ED_fileselect_is_asset_browser(sfile) || !sfile->files) {
    return nullptr;
  }

  return filelist_asset_library(sfile->files);
}

ID *ED_fileselect_active_asset_get(const SpaceFile *sfile)
{
  if (!ED_fileselect_is_asset_browser(sfile)) {
    return nullptr;
  }

  if (sfile->files == nullptr) {
    return nullptr;
  }

  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  const FileDirEntry *file = filelist_file(sfile->files, params->active_file);
  if (file == nullptr) {
    return nullptr;
  }

  return filelist_file_get_id(file);
}

void ED_fileselect_activate_asset_catalog(const SpaceFile *sfile, const bUUID catalog_id)
{
  if (!ED_fileselect_is_asset_browser(sfile)) {
    return;
  }

  FileAssetSelectParams *params = ED_fileselect_get_asset_params(sfile);
  params->asset_catalog_visibility = FILE_SHOW_ASSETS_FROM_CATALOG;
  params->catalog_id = catalog_id;
  WM_main_add_notifier(NC_SPACE | ND_SPACE_ASSET_PARAMS, nullptr);
}

int ED_fileselect_asset_import_method_get(const SpaceFile *sfile, const FileDirEntry *file)
{
  if (!ED_fileselect_is_asset_browser(sfile) || !file->asset) {
    return -1;
  }

  /* First handle the case where the asset system dictates a certain import method. */
  if (file->asset->may_override_import_method() == false) {
    BLI_assert(file->asset->get_import_method().has_value());
    return *file->asset->get_import_method();
  }

  const FileAssetSelectParams *params = ED_fileselect_get_asset_params(sfile);

  if (params->import_method == FILE_ASSET_IMPORT_FOLLOW_PREFS) {
    std::optional import_method = file->asset->get_import_method();
    return import_method ? *import_method : -1;
  }

  switch (eFileAssetImportMethod(params->import_method)) {
    case FILE_ASSET_IMPORT_LINK:
      return ASSET_IMPORT_LINK;
    case FILE_ASSET_IMPORT_APPEND:
      return ASSET_IMPORT_APPEND;
    case FILE_ASSET_IMPORT_APPEND_REUSE:
      return ASSET_IMPORT_APPEND_REUSE;
    case FILE_ASSET_IMPORT_PACK:
      return ASSET_IMPORT_PACK;

      /* Should be handled above already. Break and fail below. */
    case FILE_ASSET_IMPORT_FOLLOW_PREFS:
      break;
  }

  BLI_assert_unreachable();
  return -1;
}

static void on_reload_activate_by_id(SpaceFile *sfile, onReloadFnData custom_data)
{
  ID *asset_id = (ID *)custom_data;
  ED_fileselect_activate_by_id(sfile, asset_id, false);
}

void ED_fileselect_activate_by_id(SpaceFile *sfile, ID *asset_id, const bool deferred)
{
  if (!ED_fileselect_is_asset_browser(sfile)) {
    return;
  }

  /* If there are filelist operations running now ("pending" true) or soon ("force reset" true),
   * there is a fair chance that the to-be-activated ID will only be present after these operations
   * have completed. Defer activation until then. */
  if (deferred || filelist_pending(sfile->files) || filelist_needs_force_reset(sfile->files)) {
    /* This should be thread-safe, as this function is likely called from the main thread, and
     * notifiers (which cause a call to the on-reload callback function) are handled on the main
     * thread as well. */
    file_on_reload_callback_register(sfile, on_reload_activate_by_id, asset_id);
    return;
  }

  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileList *files = sfile->files;

  const int file_index = filelist_file_find_id(files, asset_id);
  const FileDirEntry *file = filelist_file_ex(files, file_index, true);
  if (file == nullptr) {
    return;
  }

  params->active_file = file_index;
  filelist_entry_select_set(files, file, FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);

  WM_main_add_notifier(NC_ASSET | NA_ACTIVATED, nullptr);
  WM_main_add_notifier(NC_ASSET | NA_SELECTED, nullptr);
}

static void on_reload_select_by_relpath(SpaceFile *sfile, onReloadFnData custom_data)
{
  const char *relative_path = static_cast<const char *>(custom_data);
  ED_fileselect_activate_by_relpath(sfile, relative_path);
}

void ED_fileselect_activate_by_relpath(SpaceFile *sfile, const char *relative_path)
{
  /* If there are filelist operations running now ("pending" true) or soon ("force reset" true),
   * there is a fair chance that the to-be-activated file at relative_path will only be present
   * after these operations have completed. Defer activation until then. */
  FileList *files = sfile->files;
  if (files == nullptr || filelist_pending(files) || filelist_needs_force_reset(files)) {
    /* Casting away the constness of `relative_path` is safe here, because eventually it just ends
     * up in another call to this function, and then it's a const char* again. */
    file_on_reload_callback_register(sfile, on_reload_select_by_relpath, (char *)relative_path);
    return;
  }

  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  const int num_files_filtered = filelist_files_ensure(files);

  for (int file_index = 0; file_index < num_files_filtered; ++file_index) {
    const FileDirEntry *file = filelist_file(files, file_index);

    if (STREQ(file->relpath, relative_path)) {
      params->active_file = file_index;
      filelist_entry_select_set(files, file, FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
    }
  }
  WM_main_add_notifier(NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
}

void ED_fileselect_deselect_all(SpaceFile *sfile)
{
  BLI_assert(sfile->files);
  file_select_deselect_all(sfile, FILE_SEL_SELECTED);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
}

/* The subset of FileSelectParams.flag items we store into preferences. Note that FILE_SORT_ALPHA
 * may also be remembered, but only conditionally. */
#define PARAMS_FLAGS_REMEMBERED (FILE_HIDE_DOT)

void ED_fileselect_window_params_get(const wmWindow *win, int r_win_size[2], bool *r_is_maximized)
{
  /* Get DPI/pixel-size independent size to be stored in preferences. */
  WM_window_dpi_set_userdef(win); /* Ensure the DPI is taken from the right window. */

  const blender::int2 win_size = WM_window_native_pixel_size(win);
  r_win_size[0] = win_size[0] / UI_SCALE_FAC;
  r_win_size[1] = win_size[1] / UI_SCALE_FAC;

  *r_is_maximized = WM_window_is_maximized(win);
}

static bool file_select_use_default_display_type(const SpaceFile *sfile)
{
  PropertyRNA *prop;
  return (sfile->op == nullptr) ||
         !(prop = RNA_struct_find_property(sfile->op->ptr, "display_type")) ||
         (RNA_property_enum_get(sfile->op->ptr, prop) == FILE_DEFAULTDISPLAY);
}

static bool file_select_use_default_sort_type(const SpaceFile *sfile)
{
  PropertyRNA *prop;
  return (sfile->op == nullptr) ||
         !(prop = RNA_struct_find_property(sfile->op->ptr, "sort_method")) ||
         (RNA_property_enum_get(sfile->op->ptr, prop) == FILE_SORT_DEFAULT);
}

void ED_fileselect_set_params_from_userdef(SpaceFile *sfile)
{
  wmOperator *op = sfile->op;
  UserDef_FileSpaceData *sfile_udata = &U.file_space_data;

  sfile->browse_mode = FILE_BROWSE_MODE_FILES;

  FileSelectParams *params = fileselect_ensure_updated_file_params(sfile);
  if (!op) {
    return;
  }

  params->thumbnail_size = sfile_udata->thumbnail_size;
  params->details_flags = sfile_udata->details_flags;
  params->filter_id = sfile_udata->filter_id;

  /* Combine flags we take from params with the flags we take from userdef. */
  params->flag = (params->flag & ~PARAMS_FLAGS_REMEMBERED) |
                 (sfile_udata->flag & PARAMS_FLAGS_REMEMBERED);

  if (file_select_use_default_display_type(sfile)) {
    params->display = sfile_udata->display_type;
  }
  if (file_select_use_default_sort_type(sfile)) {
    params->sort = sfile_udata->sort_type;
    /* For the default sorting, also take invert flag from userdef. */
    params->flag = (params->flag & ~FILE_SORT_INVERT) | (sfile_udata->flag & FILE_SORT_INVERT);
  }
}

void ED_fileselect_params_to_userdef(SpaceFile *sfile)
{
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  UserDef_FileSpaceData *sfile_udata_new = &U.file_space_data;
  UserDef_FileSpaceData sfile_udata_old = U.file_space_data;

  sfile_udata_new->thumbnail_size = params->thumbnail_size;
  sfile_udata_new->details_flags = params->details_flags;
  sfile_udata_new->flag = params->flag & PARAMS_FLAGS_REMEMBERED;
  sfile_udata_new->filter_id = params->filter_id;

  /* In some rare cases, operators ask for a specific display or sort type (e.g. chronological
   * sorting for "Recover Auto Save"). So the settings are optimized for a specific operation.
   * Don't let that change the userdef memory for more general cases. */
  if (file_select_use_default_display_type(sfile)) {
    sfile_udata_new->display_type = params->display;
  }
  if (file_select_use_default_sort_type(sfile)) {
    sfile_udata_new->sort_type = params->sort;
    /* In this case also remember the invert flag. */
    sfile_udata_new->flag = (sfile_udata_new->flag & ~FILE_SORT_INVERT) |
                            (params->flag & FILE_SORT_INVERT);
  }

  /* Tag preferences as dirty if something has changed. */
  if (memcmp(sfile_udata_new, &sfile_udata_old, sizeof(sfile_udata_old)) != 0) {
    U.runtime.is_dirty = true;
  }
}

void fileselect_file_set(bContext *C, SpaceFile *sfile, const int index)
{
  const FileDirEntry *file = filelist_file(sfile->files, index);
  if (file && file->relpath && file->relpath[0] && !(file->typeflag & FILE_TYPE_DIR)) {
    FileSelectParams *params = ED_fileselect_get_active_params(sfile);
    STRNCPY(params->file, file->relpath);
    if (sfile->op) {
      /* Update the filepath properties of the operator. */
      Main *bmain = CTX_data_main(C);
      file_sfile_to_operator(C, bmain, sfile->op, sfile);
    }
  }
}

int ED_fileselect_layout_numfiles(FileLayout *layout, ARegion *region)
{
  int numfiles;

  /* Values in pixels.
   *
   * - *_item: size of each (row|col), (including padding)
   * - *_view: (x|y) size of the view.
   * - *_over: extra pixels, to take into account, when the fit isn't exact
   *   (needed since you may see the end of the previous column and the beginning of the next).
   *
   * Could be more clever and take scrolling into account,
   * but for now don't bother.
   */
  if (layout->flag & FILE_LAYOUT_HOR) {
    const int x_item = layout->tile_w + (2 * layout->tile_border_x);
    const int x_view = int(BLI_rctf_size_x(&region->v2d.cur));
    const int x_over = x_item - (x_view % x_item);
    numfiles = int(float(x_view + x_over) / float(x_item));
    return numfiles * layout->rows;
  }

  const int y_item = layout->tile_h + (2 * layout->tile_border_y);
  const int y_view = int(BLI_rctf_size_y(&region->v2d.cur)) - layout->offset_top;
  const int y_over = y_item - (y_view % y_item);
  numfiles = int(float(y_view + y_over) / float(y_item));
  return numfiles * layout->flow_columns;
}

static bool is_inside(int x, int y, int cols, int rows)
{
  return ((x >= 0) && (x < cols) && (y >= 0) && (y < rows));
}

FileSelection ED_fileselect_layout_offset_rect(FileLayout *layout, const rcti *rect)
{
  int colmin, colmax, rowmin, rowmax;
  FileSelection sel;
  sel.first = sel.last = -1;

  if (layout == nullptr) {
    return sel;
  }

  colmin = (rect->xmin) / (layout->tile_w + 2 * layout->tile_border_x);
  rowmin = (rect->ymin - layout->offset_top - layout->list_padding_top) /
           (layout->tile_h + 2 * layout->tile_border_y);
  colmax = (rect->xmax) / (layout->tile_w + 2 * layout->tile_border_x);
  rowmax = (rect->ymax - layout->offset_top - layout->list_padding_top) /
           (layout->tile_h + 2 * layout->tile_border_y);

  if (is_inside(colmin, rowmin, layout->flow_columns, layout->rows) ||
      is_inside(colmax, rowmax, layout->flow_columns, layout->rows))
  {
    CLAMP(colmin, 0, layout->flow_columns - 1);
    CLAMP(rowmin, 0, layout->rows - 1);
    CLAMP(colmax, 0, layout->flow_columns - 1);
    CLAMP(rowmax, 0, layout->rows - 1);
  }

  if ((colmin > layout->flow_columns - 1) || (rowmin > layout->rows - 1)) {
    sel.first = -1;
  }
  else {
    if (layout->flag & FILE_LAYOUT_HOR) {
      sel.first = layout->rows * colmin + rowmin;
    }
    else {
      sel.first = colmin + layout->flow_columns * rowmin;
    }
  }
  if ((colmax > layout->flow_columns - 1) || (rowmax > layout->rows - 1)) {
    sel.last = -1;
  }
  else {
    if (layout->flag & FILE_LAYOUT_HOR) {
      sel.last = layout->rows * colmax + rowmax;
    }
    else {
      sel.last = colmax + layout->flow_columns * rowmax;
    }
  }

  return sel;
}

int ED_fileselect_layout_offset(FileLayout *layout, int x, int y)
{
  int offsetx, offsety;
  int active_file;

  if (layout == nullptr) {
    return -1;
  }

  offsetx = (x) / (layout->tile_w + 2 * layout->tile_border_x);
  offsety = (y - layout->offset_top - layout->list_padding_top) /
            (layout->tile_h + 2 * layout->tile_border_y);

  if (offsetx > layout->flow_columns - 1) {
    return -1;
  }
  if (offsety > layout->rows - 1) {
    return -1;
  }

  if (layout->flag & FILE_LAYOUT_HOR) {
    active_file = layout->rows * offsetx + offsety;
  }
  else {
    active_file = offsetx + layout->flow_columns * offsety;
  }
  return active_file;
}

void ED_fileselect_layout_maskrect(const FileLayout *layout, const View2D *v2d, rcti *r_rect)
{
  *r_rect = v2d->mask;
  r_rect->ymax -= layout->offset_top;
}

bool ED_fileselect_layout_is_inside_pt(const FileLayout *layout, const View2D *v2d, int x, int y)
{
  rcti maskrect;
  ED_fileselect_layout_maskrect(layout, v2d, &maskrect);
  return BLI_rcti_isect_pt(&maskrect, x, y);
}

bool ED_fileselect_layout_isect_rect(const FileLayout *layout,
                                     const View2D *v2d,
                                     const rcti *rect,
                                     rcti *r_dst)
{
  rcti maskrect;
  ED_fileselect_layout_maskrect(layout, v2d, &maskrect);
  return BLI_rcti_isect(&maskrect, rect, r_dst);
}

void ED_fileselect_layout_tilepos(const FileLayout *layout, int tile, int *x, int *y)
{
  if (layout->flag == FILE_LAYOUT_HOR) {
    *x = layout->tile_border_x +
         (tile / layout->rows) * (layout->tile_w + 2 * layout->tile_border_x);
    *y = layout->offset_top + layout->list_padding_top + layout->tile_border_y +
         (tile % layout->rows) * (layout->tile_h + 2 * layout->tile_border_y);
  }
  else {
    *x = layout->tile_border_x +
         ((tile) % layout->flow_columns) * (layout->tile_w + 2 * layout->tile_border_x);
    *y = layout->offset_top + layout->list_padding_top + layout->tile_border_y +
         ((tile) / layout->flow_columns) * (layout->tile_h + 2 * layout->tile_border_y);
  }
}

bool file_attribute_column_header_is_inside(const View2D *v2d,
                                            const FileLayout *layout,
                                            int x,
                                            int y)
{
  rcti header_rect = v2d->mask;
  header_rect.ymin = header_rect.ymax - layout->attribute_column_header_h;
  return BLI_rcti_isect_pt(&header_rect, x, y);
}

bool file_attribute_column_type_enabled(const FileSelectParams *params,
                                        FileAttributeColumnType column,
                                        const FileLayout *layout)
{
  switch (column) {
    case COLUMN_NAME:
      /* Always enabled */
      return true;
    case COLUMN_DATETIME:
      return ((params->details_flags & FILE_DETAILS_DATETIME) != 0) &&
             !FILE_LAYOUT_HIDE_DATE(layout);
    case COLUMN_SIZE:
      return ((params->details_flags & FILE_DETAILS_SIZE) != 0) && !FILE_LAYOUT_HIDE_SIZE(layout);
    default:
      return false;
  }
}

FileAttributeColumnType file_attribute_column_type_find_isect(const View2D *v2d,
                                                              const FileSelectParams *params,
                                                              FileLayout *layout,
                                                              int x)
{
  float mx, my;
  int offset_tile;

  UI_view2d_region_to_view(v2d, x, v2d->mask.ymax - layout->offset_top - 1, &mx, &my);
  offset_tile = ED_fileselect_layout_offset(
      layout, int(v2d->tot.xmin + mx), int(v2d->tot.ymax - my));
  if (offset_tile > -1) {
    int tile_x, tile_y;
    int pos_x = 0;
    int rel_x; /* x relative to the hovered tile */

    ED_fileselect_layout_tilepos(layout, offset_tile, &tile_x, &tile_y);
    /* Column header drawing doesn't use left tile border, so subtract it. */
    rel_x = mx - (tile_x - layout->tile_border_x);

    for (FileAttributeColumnType column = FileAttributeColumnType(0);
         column < ATTRIBUTE_COLUMN_MAX;
         column = FileAttributeColumnType(int(column) + 1))
    {
      if (!file_attribute_column_type_enabled(params, column, layout)) {
        continue;
      }
      const int width = layout->attribute_columns[column].width;

      if (IN_RANGE(rel_x, pos_x, pos_x + width)) {
        return column;
      }

      pos_x += width;
    }
  }

  return COLUMN_NONE;
}

float file_string_width(const char *str)
{
  const uiStyle *style = UI_style_get();
  UI_fontstyle_set(&style->widget);
  return BLF_width(style->widget.uifont_id, str, BLF_DRAW_STR_DUMMY_MAX);
}

float file_font_pointsize()
{
  const uiStyle *style = UI_style_get();
  return UI_fontstyle_height_max(&style->widget);
}

static void file_attribute_columns_widths(const FileSelectParams *params, FileLayout *layout)
{
  FileAttributeColumn *columns = layout->attribute_columns;
  const int pad = ATTRIBUTE_COLUMN_PADDING * 2;
  const bool compact = FILE_LAYOUT_COMPACT(layout);

  for (int i = 0; i < ATTRIBUTE_COLUMN_MAX; i++) {
    layout->attribute_columns[i].width = 0;
  }

  /* Biggest possible reasonable values... */
  if (file_attribute_column_type_enabled(params, COLUMN_DATETIME, layout)) {
    columns[COLUMN_DATETIME].width = file_string_width(compact ? "23/08/89" :
                                                                 "23 Dec 6789, 23:59") +
                                     pad;
  }
  if (file_attribute_column_type_enabled(params, COLUMN_SIZE, layout)) {
    columns[COLUMN_SIZE].width = file_string_width(compact ? "369G" : "098.7 MiB") + pad;
  }

  if (params->display == FILE_IMGDISPLAY) {
    columns[COLUMN_NAME].width = (float(params->thumbnail_size) / 8.0f) * UI_UNIT_X;
  }
  /* Name column uses remaining width */
  else {
    int remwidth = layout->tile_w;
    for (FileAttributeColumnType column_type =
             FileAttributeColumnType(int(ATTRIBUTE_COLUMN_MAX) - 1);
         column_type >= 0;
         column_type = FileAttributeColumnType(int(column_type) - 1))
    {
      if ((column_type == COLUMN_NAME) ||
          !file_attribute_column_type_enabled(params, column_type, layout))
      {
        continue;
      }
      remwidth -= columns[column_type].width;
    }
    columns[COLUMN_NAME].width = remwidth;
  }
}

static void file_attribute_columns_init(const FileSelectParams *params, FileLayout *layout)
{
  file_attribute_columns_widths(params, layout);

  layout->attribute_columns[COLUMN_NAME].name = N_("Name");
  layout->attribute_columns[COLUMN_NAME].sort_type = FILE_SORT_ALPHA;
  layout->attribute_columns[COLUMN_NAME].text_align = UI_STYLE_TEXT_LEFT;

  const bool compact = FILE_LAYOUT_COMPACT(layout);
  layout->attribute_columns[COLUMN_DATETIME].name = compact ? N_("Date") : N_("Date Modified");

  layout->attribute_columns[COLUMN_DATETIME].sort_type = FILE_SORT_TIME;
  layout->attribute_columns[COLUMN_DATETIME].text_align = UI_STYLE_TEXT_LEFT;
  layout->attribute_columns[COLUMN_SIZE].name = N_("Size");
  layout->attribute_columns[COLUMN_SIZE].sort_type = FILE_SORT_SIZE;
  layout->attribute_columns[COLUMN_SIZE].text_align = UI_STYLE_TEXT_RIGHT;
}

void ED_fileselect_init_layout(SpaceFile *sfile, ARegion *region)
{
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileLayout *layout = nullptr;
  View2D *v2d = &region->v2d;
  int numfiles;

  if (sfile->layout == nullptr) {
    sfile->layout = MEM_callocN<FileLayout>("file_layout");
    sfile->layout->dirty = true;
  }
  else if (sfile->layout->dirty == false) {
    return;
  }

  numfiles = filelist_files_ensure(sfile->files);
  layout = sfile->layout;
  /* Slightly increased than font height for padding. */
  layout->text_line_height = file_font_pointsize();
  layout->text_lines_count = 1;

  if (params->display == FILE_IMGDISPLAY) {
    /* More compact spacing for asset browser. */
    const float pad_fac = ED_fileselect_is_asset_browser(sfile) ? 0.15f : 0.3f;
    /* Matches UI_preview_tile_size_x()/_y() by default. */
    layout->prv_w = (float(params->thumbnail_size) / 20.0f) * UI_UNIT_X;
    layout->prv_h = (float(params->thumbnail_size) / 20.0f) * UI_UNIT_Y;
    layout->tile_border_x = pad_fac * UI_UNIT_X;
    layout->tile_border_y = pad_fac * UI_UNIT_X;
    layout->list_padding_top = 0;
    layout->prv_border_x = pad_fac * UI_UNIT_X;
    layout->prv_border_y = pad_fac * UI_UNIT_Y;
    layout->tile_w = layout->prv_w + 2 * layout->prv_border_x;
    layout->text_lines_count = 2;
    layout->tile_h = layout->prv_h + 2 * layout->prv_border_y +
                     layout->text_lines_count * layout->text_line_height;
    layout->width = int(BLI_rctf_size_x(&v2d->cur) - 2 * layout->tile_border_x);
    layout->flow_columns = layout->width / (layout->tile_w + 2 * layout->tile_border_x);
    layout->attribute_column_header_h = 0;
    layout->offset_top = 0;
    if (layout->flow_columns > 0) {
      layout->rows = divide_ceil_u(numfiles, layout->flow_columns);
    }
    else {
      layout->flow_columns = 1;
      layout->rows = numfiles;
    }
    layout->height = sfile->layout->rows * (layout->tile_h + 2 * layout->tile_border_y) +
                     layout->tile_border_y * 2 - layout->offset_top;
    layout->flag = FILE_LAYOUT_VER;
  }
  else if (params->display == FILE_VERTICALDISPLAY) {
    int rowcount;

    layout->prv_w = ICON_DEFAULT_WIDTH_SCALE;
    layout->prv_h = ICON_DEFAULT_HEIGHT_SCALE;
    layout->tile_border_x = 0.4f * UI_UNIT_X;
    layout->tile_border_y = 0.05f * UI_UNIT_Y;
    layout->list_padding_top = 2 * layout->tile_border_y;
    layout->tile_h = round_fl_to_int(layout->text_line_height * 1.4f);
    layout->width = int(BLI_rctf_size_x(&v2d->cur) - 2 * layout->tile_border_x);
    layout->tile_w = layout->width;
    layout->flow_columns = 1;
    layout->attribute_column_header_h = layout->tile_h * 1.2f + 2 * layout->tile_border_y;
    layout->offset_top = layout->attribute_column_header_h;
    rowcount = int(BLI_rctf_size_y(&v2d->cur) - layout->offset_top -
                   2 * layout->list_padding_top) /
               (layout->tile_h + 2 * layout->tile_border_y);
    file_attribute_columns_init(params, layout);

    layout->rows = std::max(rowcount, numfiles);

    /* layout->rows can be zero if a very small area is changed to a File Browser. #124168. */

    layout->height = sfile->layout->rows * (layout->tile_h + 2 * layout->tile_border_y) +
                     layout->list_padding_top * 2 + layout->offset_top;
    layout->flag = FILE_LAYOUT_VER;
  }
  else if (params->display == FILE_HORIZONTALDISPLAY) {
    layout->prv_w = params->list_thumbnail_size * UI_SCALE_FAC;
    layout->prv_h = params->list_thumbnail_size * UI_SCALE_FAC;
    layout->tile_border_x = 0.4f * UI_UNIT_X;
    layout->tile_border_y = 0.05f * UI_UNIT_Y;
    layout->list_padding_top = 2 * layout->tile_border_y;
    layout->tile_h = std::max(round_fl_to_int(layout->text_line_height * 1.4f), layout->prv_h);
    layout->attribute_column_header_h = 0;
    layout->offset_top = layout->attribute_column_header_h;
    layout->height = int(BLI_rctf_size_y(&v2d->cur) - 2 * layout->tile_border_y);
    /* Padding by full scroll-bar H is too much, can overlap tile border Y. */
    layout->rows = (layout->height - V2D_SCROLL_HEIGHT + layout->tile_border_y) /
                   (layout->tile_h + 2 * layout->tile_border_y);

    layout->tile_w = params->list_column_size * UI_SCALE_FAC;
    file_attribute_columns_init(params, layout);

    if (layout->rows > 0) {
      layout->flow_columns = divide_ceil_u(numfiles, layout->rows);
    }
    else {
      layout->rows = 1;
      layout->flow_columns = numfiles;
    }
    layout->width = (numfiles > 0) ? (sfile->layout->flow_columns *
                                          (layout->tile_w + 2 * layout->tile_border_x) +
                                      layout->tile_border_x * 2) :
                                     int(BLI_rctf_size_x(&v2d->cur) - 2 * layout->tile_border_x);
    layout->flag = FILE_LAYOUT_HOR;
  }
  layout->dirty = false;
}

FileLayout *ED_fileselect_get_layout(SpaceFile *sfile, ARegion *region)
{
  if (!sfile->layout) {
    ED_fileselect_init_layout(sfile, region);
  }
  return sfile->layout;
}

void ED_file_change_dir_ex(bContext *C, ScrArea *area)
{
  /* May happen when manipulating non-active spaces. */
  if (UNLIKELY(area->spacetype != SPACE_FILE)) {
    return;
  }
  SpaceFile *sfile = static_cast<SpaceFile *>(area->spacedata.first);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  if (params) {
    wmWindowManager *wm = CTX_wm_manager(C);
    ED_fileselect_clear(wm, sfile);

    /* Clear search string, it is very rare to want to keep that filter while changing dir,
     * and usually very annoying to keep it actually! */
    params->filter_search[0] = '\0';
    params->active_file = -1;

    if (!filelist_is_dir(sfile->files, params->dir)) {
      STRNCPY(params->dir, filelist_dir(sfile->files));
      /* could return but just refresh the current dir */
    }
    filelist_setdir(sfile->files, params->dir);

    if (folderlist_clear_next(sfile)) {
      folderlist_free(sfile->folders_next);
    }

    folderlist_pushdir(sfile->folders_prev, params->dir);

    file_draw_check_ex(C, area);
  }
}

void ED_file_change_dir(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  ED_file_change_dir_ex(C, area);
}

void file_select_deselect_all(SpaceFile *sfile, const eDirEntry_SelectFlag flag)
{
  FileSelection sel;
  sel.first = 0;
  sel.last = filelist_files_ensure(sfile->files) - 1;

  filelist_entries_select_index_range_set(sfile->files, &sel, FILE_SEL_REMOVE, flag, CHECK_ALL);
}

int file_select_match(SpaceFile *sfile, const char *pattern, char *matched_file)
{
  int match = 0;

  int n = filelist_files_ensure(sfile->files);

  /* select any file that matches the pattern, this includes exact match
   * if the user selects a single file by entering the filename
   */
  for (int i = 0; i < n; i++) {
    const char *relpath = filelist_entry_get_relpath(sfile->files, i);
    /* Do not check whether file is a file or dir here! Causes: #44243
     * (we do accept directories at this stage). */
    if (fnmatch(pattern, relpath, 0) == 0) {
      filelist_entry_select_index_set(sfile->files, i, FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);
      if (!match) {
        BLI_strncpy(matched_file, relpath, FILE_MAX);
      }
      match++;
    }
  }

  return match;
}

int autocomplete_directory(bContext *C, char *str, void * /*arg_v*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  int match = AUTOCOMPLETE_NO_MATCH;

  /* search if str matches the beginning of name */
  if (str[0] && sfile->files) {
    char dirname[FILE_MAX];

    DIR *dir;
    dirent *de;

    BLI_path_split_dir_part(str, dirname, sizeof(dirname));

    BLI_assert(!BLI_path_is_rel(dirname));
    dir = opendir(dirname);

    if (dir) {
      AutoComplete *autocpl = UI_autocomplete_begin(str, FILE_MAX);

      while ((de = readdir(dir)) != nullptr) {
        if (FILENAME_IS_CURRPAR(de->d_name)) {
          /* pass */
        }
        else {
          char dirpath[FILE_MAX];
          BLI_stat_t status;

          BLI_path_join(dirpath, sizeof(dirpath), dirname, de->d_name);

          if (BLI_stat(dirpath, &status) == 0) {
            if (S_ISDIR(status.st_mode)) { /* is subdir */
              UI_autocomplete_update_name(autocpl, dirpath);
            }
          }
        }
      }
      closedir(dir);

      match = UI_autocomplete_end(autocpl, str);
      if (match == AUTOCOMPLETE_FULL_MATCH) {
        BLI_path_slash_ensure(str, FILE_MAX);
      }
    }
  }

  return match;
}

int autocomplete_file(bContext *C, char *str, void * /*arg_v*/)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  int match = AUTOCOMPLETE_NO_MATCH;

  /* search if str matches the beginning of name */
  if (str[0] && sfile->files) {
    AutoComplete *autocpl = UI_autocomplete_begin(str, FILE_MAX);
    int nentries = filelist_files_ensure(sfile->files);

    for (int i = 0; i < nentries; i++) {
      const char *relpath = filelist_entry_get_relpath(sfile->files, i);
      UI_autocomplete_update_name(autocpl, relpath);
    }
    match = UI_autocomplete_end(autocpl, str);
  }

  return match;
}

void ED_fileselect_clear(wmWindowManager *wm, SpaceFile *sfile)
{
  /* Only null in rare cases, see: #29734. */
  if (sfile->files) {
    filelist_readjob_stop(sfile->files, wm);
    filelist_freelib(sfile->files);
    filelist_clear(sfile->files);
  }

  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  params->highlight_file = -1;
  WM_main_add_notifier(NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
}

void ED_fileselect_clear_main_assets(wmWindowManager *wm, SpaceFile *sfile)
{
  /* Only null in rare cases, see: #29734. */
  if (sfile->files) {
    filelist_readjob_stop(sfile->files, wm);
    filelist_freelib(sfile->files);
    filelist_tag_force_reset_mainfiles(sfile->files);
    filelist_tag_reload_asset_library(sfile->files);
    filelist_clear_from_reset_tag(sfile->files);
  }

  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  params->highlight_file = -1;
  WM_main_add_notifier(NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
}

void ED_fileselect_exit(wmWindowManager *wm, SpaceFile *sfile)
{
  if (!sfile) {
    return;
  }
  if (sfile->op) {
    ED_fileselect_params_to_userdef(sfile);
    WM_event_fileselect_event(wm, sfile->op, EVT_FILESELECT_EXTERNAL_CANCEL);
    sfile->op = nullptr;
  }

  folder_history_list_free(sfile);

  if (sfile->files) {
    ED_fileselect_clear(wm, sfile);
    filelist_free(sfile->files);
    sfile->files = nullptr;
  }
}

void file_params_smoothscroll_timer_clear(wmWindowManager *wm, wmWindow *win, SpaceFile *sfile)
{
  WM_event_timer_remove(wm, win, sfile->smoothscroll_timer);
  sfile->smoothscroll_timer = nullptr;
}

void file_params_invoke_rename_postscroll(wmWindowManager *wm, wmWindow *win, SpaceFile *sfile)
{
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  params->rename_flag = FILE_PARAMS_RENAME_POSTSCROLL_PENDING;

  if (sfile->smoothscroll_timer != nullptr) {
    file_params_smoothscroll_timer_clear(wm, win, sfile);
  }
  sfile->smoothscroll_timer = WM_event_timer_add(wm, win, TIMER1, 1.0 / 1000.0);
  sfile->scroll_offset = 0;
}

void file_params_rename_end(wmWindowManager *wm,
                            wmWindow *win,
                            SpaceFile *sfile,
                            const FileDirEntry *rename_file)
{
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  filelist_entry_select_set(
      sfile->files, rename_file, FILE_SEL_REMOVE, FILE_SEL_EDITING, CHECK_ALL);

  /* Ensure smooth-scroll timer is active, even if not needed, because that way rename state is
   * handled properly. */
  file_params_invoke_rename_postscroll(wm, win, sfile);
  /* Also always activate the rename file, even if renaming was canceled. */
  file_params_renamefile_activate(sfile, params);
}

void file_params_renamefile_clear(FileSelectParams *params)
{
  params->renamefile[0] = '\0';
  params->rename_id = nullptr;
  params->rename_flag = 0;
}

static int file_params_find_renamed(const FileSelectParams *params, FileList *filelist)
{
  /* Find the file either through the local ID/asset it represents or its relative path. */
  return (params->rename_id != nullptr) ? filelist_file_find_id(filelist, params->rename_id) :
                                          filelist_file_find_path(filelist, params->renamefile);
}

void file_params_renamefile_activate(SpaceFile *sfile, FileSelectParams *params)
{
  BLI_assert(params->rename_flag != 0);

  if ((params->rename_flag & (FILE_PARAMS_RENAME_ACTIVE | FILE_PARAMS_RENAME_POSTSCROLL_ACTIVE)) !=
      0)
  {
    return;
  }

  BLI_assert(params->renamefile[0] != '\0' || params->rename_id != nullptr);

  int idx = file_params_find_renamed(params, sfile->files);
  if (idx >= 0) {
    FileDirEntry *file = filelist_file(sfile->files, idx);
    BLI_assert(file != nullptr);

    params->active_file = idx;
    filelist_entry_select_set(sfile->files, file, FILE_SEL_ADD, FILE_SEL_SELECTED, CHECK_ALL);

    if ((params->rename_flag & FILE_PARAMS_RENAME_PENDING) != 0) {
      filelist_entry_select_set(sfile->files, file, FILE_SEL_ADD, FILE_SEL_EDITING, CHECK_ALL);
      params->rename_flag = FILE_PARAMS_RENAME_ACTIVE;
    }
    else if ((params->rename_flag & FILE_PARAMS_RENAME_POSTSCROLL_PENDING) != 0) {
      /* file_select_deselect_all() will resort and re-filter, so `idx` will probably have changed.
       * Need to get the correct #FileDirEntry again. */
      file_select_deselect_all(sfile, FILE_SEL_SELECTED);
      idx = file_params_find_renamed(params, sfile->files);
      file = filelist_file(sfile->files, idx);
      filelist_entry_select_set(
          sfile->files, file, FILE_SEL_ADD, FILE_SEL_SELECTED | FILE_SEL_HIGHLIGHTED, CHECK_ALL);
      params->active_file = idx;
      file_params_renamefile_clear(params);
      params->rename_flag = FILE_PARAMS_RENAME_POSTSCROLL_ACTIVE;
    }
  }
  /* File listing is now asynchronous, only reset renaming if matching entry is not found
   * when file listing is not done. */
  else if (filelist_is_ready(sfile->files)) {
    file_params_renamefile_clear(params);
  }
}

ScrArea *ED_fileselect_handler_area_find(const wmWindow *win, const wmOperator *file_operator)
{
  bScreen *screen = WM_window_get_active_screen(win);

  ED_screen_areas_iter (win, screen, area) {
    if (area->spacetype == SPACE_FILE) {
      SpaceFile *sfile = static_cast<SpaceFile *>(area->spacedata.first);

      if (sfile->op == file_operator) {
        return area;
      }
    }
  }

  return nullptr;
}

ScrArea *ED_fileselect_handler_area_find_any_with_op(const wmWindow *win)
{
  const bScreen *screen = WM_window_get_active_screen(win);

  ED_screen_areas_iter (win, screen, area) {
    if (area->spacetype != SPACE_FILE) {
      continue;
    }

    const SpaceFile *sfile = static_cast<SpaceFile *>(area->spacedata.first);
    if (sfile->op) {
      return area;
    }
  }

  return nullptr;
}

void ED_fileselect_ensure_default_filepath(bContext *C, wmOperator *op, const char *extension)
{
  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    Main *bmain = CTX_data_main(C);
    char filepath[FILE_MAX];
    const char *blendfile_path = BKE_main_blendfile_path(bmain);

    if (blendfile_path[0] == '\0') {
      STRNCPY(filepath, DATA_("Untitled"));
    }
    else {
      STRNCPY(filepath, blendfile_path);
    }

    BLI_path_extension_replace(filepath, sizeof(filepath), extension);
    RNA_string_set(op->ptr, "filepath", filepath);
  }
}

blender::Vector<std::string> ED_fileselect_selected_files_full_paths(const SpaceFile *sfile)
{
  blender::Vector<std::string> paths;
  char path[FILE_MAX_LIBEXTRA];
  for (const int i : blender::IndexRange(filelist_files_ensure(sfile->files))) {
    if (filelist_entry_is_selected(sfile->files, i)) {
      const FileDirEntry *entry = filelist_file(sfile->files, i);
      filelist_file_get_full_path(sfile->files, entry, path);
      paths.append(path);
    }
  }
  return paths;
}
