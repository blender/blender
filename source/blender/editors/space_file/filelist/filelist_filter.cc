/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include "AS_asset_representation.hh"

#include "BLI_fnmatch.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_idtype.hh"

#include "../file_intern.hh"
#include "../filelist.hh"
#include "filelist_intern.hh"

using namespace blender;

/* True if should be hidden, based on current filtering. */
static bool is_filtered_hidden(const char *filename,
                               const FileListFilter *filter,
                               const FileListInternEntry *file)
{
  if ((filename[0] == '.') && (filename[1] == '\0')) {
    return true; /* Ignore. */
  }

  if (filter->flags & FLF_HIDE_PARENT) {
    if (filename[0] == '.' && filename[1] == '.' && filename[2] == '\0') {
      return true; /* Ignore. */
    }
  }

  /* Check for _OUR_ "hidden" attribute. This not only mirrors OS-level hidden file
   * attribute but is also set for Linux/Mac "dot" files. See `filelist_readjob_list_dir`.
   */
  if ((filter->flags & FLF_HIDE_DOT) && (file->attributes & FILE_ATTR_HIDDEN)) {
    return true;
  }

  /* For data-blocks (but not the group directories), check the asset-only filter. */
  if (!(file->typeflag & FILE_TYPE_DIR) && (file->typeflag & FILE_TYPE_BLENDERLIB) &&
      (filter->flags & FLF_ASSETS_ONLY) && !(file->typeflag & FILE_TYPE_ASSET))
  {
    return true;
  }

  return false;
}

/**
 * Apply the filter string as file path matching pattern.
 * \return true when the file should be in the result set, false if it should be filtered out.
 */
static bool is_filtered_file_relpath(const FileListInternEntry *file, const FileListFilter *filter)
{
  if (filter->filter_search[0] == '\0') {
    return true;
  }

  /* If there's a filter string, apply it as filter even if FLF_DO_FILTER is not set. */
  return fnmatch(filter->filter_search, file->relpath, FNM_CASEFOLD) == 0;
}

/**
 * Apply the filter string as matching pattern on file name.
 * \return true when the file should be in the result set, false if it should be filtered out.
 */
static bool is_filtered_file_name(const FileListInternEntry *file, const FileListFilter *filter)
{
  if (filter->filter_search[0] == '\0') {
    return true;
  }

  /* If there's a filter string, apply it as filter even if FLF_DO_FILTER is not set. */
  return fnmatch(filter->filter_search, file->name, FNM_CASEFOLD) == 0;
}

/** \return true when the file should be in the result set, false if it should be filtered out. */
static bool is_filtered_file_type(const FileListInternEntry *file, const FileListFilter *filter)
{
  if (is_filtered_hidden(file->relpath, filter, file)) {
    return false;
  }

  if (FILENAME_IS_CURRPAR(file->relpath)) {
    return false;
  }

  /* We only check for types if some type are enabled in filtering. */
  if (filter->filter && (filter->flags & FLF_DO_FILTER)) {
    if (file->typeflag & FILE_TYPE_DIR) {
      if (file->typeflag & (FILE_TYPE_BLENDERLIB | FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
        if (!(filter->filter & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP))) {
          return false;
        }
      }
      else {
        if (!(filter->filter & FILE_TYPE_FOLDER)) {
          return false;
        }
      }
    }
    else {
      if (!(file->typeflag & filter->filter)) {
        return false;
      }
    }
  }
  return true;
}

bool is_filtered_file(FileListInternEntry *file, const char * /*root*/, FileListFilter *filter)
{
  return is_filtered_file_type(file, filter) &&
         (is_filtered_file_relpath(file, filter) || is_filtered_file_name(file, filter));
}

static bool is_filtered_id_file_type(const FileListInternEntry *file,
                                     const short id_code,
                                     const char *name,
                                     const FileListFilter *filter)
{
  if (!is_filtered_file_type(file, filter)) {
    return false;
  }

  /* We only check for types if some type are enabled in filtering. */
  if ((filter->filter || filter->filter_id) && (filter->flags & FLF_DO_FILTER)) {
    if (id_code) {
      if (!name && (filter->flags & FLF_HIDE_LIB_DIR)) {
        return false;
      }

      const uint64_t filter_id = BKE_idtype_idcode_to_idfilter(id_code);
      if (!(filter_id & filter->filter_id)) {
        return false;
      }
    }
  }

  return true;
}

/**
 * Get the asset metadata of a file, if it represents an asset. This may either be of a local ID
 * (ID in the current #Main) or read from an external asset library.
 */
static AssetMetaData *filelist_file_internal_get_asset_data(const FileListInternEntry *file)
{
  if (asset_system::AssetRepresentation *asset = file->get_asset()) {
    return &asset->get_metadata();
  }
  return nullptr;
}

void prepare_filter_asset_library(const FileList *filelist, FileListFilter *filter)
{
  /* Not used yet for the asset view template. */
  if (!filter->asset_catalog_filter) {
    return;
  }
  BLI_assert_msg(filelist->asset_library,
                 "prepare_filter_asset_library() should only be called when the file browser is "
                 "in asset browser mode");

  file_ensure_updated_catalog_filter_data(filter->asset_catalog_filter, filelist->asset_library);
}

/**
 * Return whether at least one tag matches the search filter.
 * Tags are searched as "entire words", so instead of searching for "tag" in the
 * filter string, this function searches for " tag ". Assumes the search filter
 * starts and ends with a space.
 *
 * Here the tags on the asset are written in set notation:
 *
 * `asset_tag_matches_filter(" some tags ", {"some", "blue"})` -> true
 * `asset_tag_matches_filter(" some tags ", {"som", "tag"})` -> false
 * `asset_tag_matches_filter(" some tags ", {})` -> false
 */
static bool asset_tag_matches_filter(const char *filter_search, const AssetMetaData *asset_data)
{
  LISTBASE_FOREACH (const AssetTag *, asset_tag, &asset_data->tags) {
    if (BLI_strcasestr(asset_tag->name, filter_search) != nullptr) {
      return true;
    }
  }
  return false;
}

bool is_filtered_asset(FileListInternEntry *file, FileListFilter *filter)
{
  const AssetMetaData *asset_data = filelist_file_internal_get_asset_data(file);

  /* Not used yet for the asset view template. */
  if (filter->asset_catalog_filter &&
      !file_is_asset_visible_in_catalog_filter_settings(filter->asset_catalog_filter, asset_data))
  {
    return false;
  }

  if (filter->filter_search[0] == '\0') {
    /* If there is no filter text, everything matches. */
    return true;
  }

  /* filter->filter_search contains "*the search text*". */
  char filter_search[sizeof(FileListFilter::filter_search)];
  const size_t string_length = STRNCPY_RLEN(filter_search, filter->filter_search);

  /* When doing a name comparison, get rid of the leading/trailing asterisks. */
  filter_search[string_length - 1] = '\0';
  if (BLI_strcasestr(file->name, filter_search + 1) != nullptr) {
    return true;
  }
  return asset_tag_matches_filter(filter_search + 1, asset_data);
}

static bool is_filtered_lib_type(FileListInternEntry *file,
                                 const char * /*root*/,
                                 FileListFilter *filter)
{
  if (file->typeflag & FILE_TYPE_BLENDERLIB) {
    return is_filtered_id_file_type(file, file->blentype, file->name, filter);
  }
  return is_filtered_file_type(file, filter);
}

bool is_filtered_lib(FileListInternEntry *file, const char *root, FileListFilter *filter)
{
  return is_filtered_lib_type(file, root, filter) && is_filtered_file_relpath(file, filter);
}

bool is_filtered_main(FileListInternEntry *file, const char * /*dir*/, FileListFilter *filter)
{
  return !is_filtered_hidden(file->relpath, filter, file);
}

bool is_filtered_main_assets(FileListInternEntry *file,
                             const char * /*dir*/,
                             FileListFilter *filter)
{
  /* "Filtered" means *not* being filtered out... So return true if the file should be visible. */
  return is_filtered_id_file_type(file, file->blentype, file->name, filter) &&
         is_filtered_asset(file, filter);
}

bool is_filtered_asset_library(FileListInternEntry *file, const char *root, FileListFilter *filter)
{
  if (filelist_intern_entry_is_main_file(file)) {
    return is_filtered_main_assets(file, root, filter);
  }

  return is_filtered_lib_type(file, root, filter) && is_filtered_asset(file, filter);
}

void filelist_tag_needs_filtering(FileList *filelist)
{
  filelist->flags |= FL_NEED_FILTERING;
}

bool filelist_needs_filtering(FileList *filelist)
{
  return (filelist->flags & FL_NEED_FILTERING);
}

void filelist_filter(FileList *filelist)
{
  int num_filtered = 0;
  const int num_files = filelist->filelist.entries_num;
  FileListInternEntry **filtered_tmp;

  if (ELEM(filelist->filelist.entries_num, FILEDIR_NBR_ENTRIES_UNSET, 0)) {
    return;
  }

  if (!(filelist->flags & FL_NEED_FILTERING)) {
    /* Assume it has already been filtered, nothing else to do! */
    return;
  }

  filelist->filter_data.flags &= ~FLF_HIDE_LIB_DIR;
  if (filelist->max_recursion) {
    /* Never show lib ID 'categories' directories when we are in 'flat' mode, unless
     * root path is a blend file. */
    char dir[FILE_MAX_LIBEXTRA];
    if (!filelist_islibrary(filelist, dir, nullptr)) {
      filelist->filter_data.flags |= FLF_HIDE_LIB_DIR;
    }
  }

  if (filelist->prepare_filter_fn) {
    filelist->prepare_filter_fn(filelist, &filelist->filter_data);
  }

  filtered_tmp = static_cast<FileListInternEntry **>(
      MEM_mallocN(sizeof(*filtered_tmp) * size_t(num_files), __func__));

  /* Filter remap & count how many files are left after filter in a single loop. */
  LISTBASE_FOREACH (FileListInternEntry *, file, &filelist->filelist_intern.entries) {
    if (filelist->filter_fn(file, filelist->filelist.root, &filelist->filter_data)) {
      filtered_tmp[num_filtered++] = file;
    }
  }

  if (filelist->filelist_intern.filtered) {
    MEM_freeN(filelist->filelist_intern.filtered);
  }
  filelist->filelist_intern.filtered = static_cast<FileListInternEntry **>(
      MEM_mallocN(sizeof(*filelist->filelist_intern.filtered) * size_t(num_filtered), __func__));
  memcpy(filelist->filelist_intern.filtered,
         filtered_tmp,
         sizeof(*filelist->filelist_intern.filtered) * size_t(num_filtered));
  filelist->filelist.entries_filtered_num = num_filtered;
  //  printf("Filtered: %d over %d entries\n", num_filtered, filelist->filelist.entries_num);

  filelist_cache_clear(filelist->filelist_cache, filelist->filelist_cache->size);
  filelist->flags &= ~FL_NEED_FILTERING;

  MEM_freeN(filtered_tmp);
}

void filelist_setfilter_options(FileList *filelist,
                                const bool do_filter,
                                const bool hide_dot,
                                const bool hide_parent,
                                const uint64_t filter,
                                const uint64_t filter_id,
                                const bool filter_assets_only,
                                const char *filter_glob,
                                const char *filter_search)
{
  bool update = false;

  if (((filelist->filter_data.flags & FLF_DO_FILTER) != 0) != (do_filter != 0)) {
    filelist->filter_data.flags ^= FLF_DO_FILTER;
    update = true;
  }
  if (((filelist->filter_data.flags & FLF_HIDE_DOT) != 0) != (hide_dot != 0)) {
    filelist->filter_data.flags ^= FLF_HIDE_DOT;
    update = true;
  }
  if (((filelist->filter_data.flags & FLF_HIDE_PARENT) != 0) != (hide_parent != 0)) {
    filelist->filter_data.flags ^= FLF_HIDE_PARENT;
    update = true;
  }
  if (((filelist->filter_data.flags & FLF_ASSETS_ONLY) != 0) != (filter_assets_only != 0)) {
    filelist->filter_data.flags ^= FLF_ASSETS_ONLY;
    update = true;
  }
  if (filelist->filter_data.filter != filter) {
    filelist->filter_data.filter = filter;
    update = true;
  }
  const uint64_t new_filter_id = (filter & FILE_TYPE_BLENDERLIB) ? filter_id : FILTER_ID_ALL;
  if (filelist->filter_data.filter_id != new_filter_id) {
    filelist->filter_data.filter_id = new_filter_id;
    update = true;
  }
  if (!STREQ(filelist->filter_data.filter_glob, filter_glob)) {
    STRNCPY_UTF8(filelist->filter_data.filter_glob, filter_glob);
    update = true;
  }
  if (BLI_strcmp_ignore_pad(filelist->filter_data.filter_search, filter_search, '*') != 0) {
    BLI_strncpy_ensure_pad(filelist->filter_data.filter_search,
                           filter_search,
                           '*',
                           sizeof(filelist->filter_data.filter_search));
    update = true;
  }

  if (update) {
    /* And now, free filtered data so that we know we have to filter again. */
    filelist_tag_needs_filtering(filelist);
  }
}
