/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

/* global includes */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
#include <sys/stat.h>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <direct.h>
#  include <io.h>
#endif

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "MEM_guardedalloc.h"

#include "BLF_api.hh"

#include "BLI_enum_flags.hh"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_stack.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_task.h"
#include "BLI_threads.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BKE_asset.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_idtype.hh"
#include "BKE_main.hh"
#include "BKE_preferences.h"
#include "BKE_preview_image.hh"

#include "DNA_asset_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "ED_fileselect.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_thumbs.hh"

#include "MOV_util.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "atomic_ops.h"

#include "../file_indexer.hh"
#include "../file_intern.hh"
#include "../filelist.hh"
#include "filelist_intern.hh"

using namespace blender;

static ImBuf *gSpecialFileImages[int(SpecialFileImages::_Max)];

static void filelist_readjob_main(FileListReadJob *job_params,
                                  bool *stop,
                                  bool *do_update,
                                  float *progress);
static void filelist_readjob_lib(FileListReadJob *job_params,
                                 bool *stop,
                                 bool *do_update,
                                 float *progress);
static void filelist_readjob_dir(FileListReadJob *job_params,
                                 bool *stop,
                                 bool *do_update,
                                 float *progress);
static void filelist_readjob_asset_library(FileListReadJob *job_params,
                                           bool *stop,
                                           bool *do_update,
                                           float *progress);
static void filelist_readjob_main_assets(FileListReadJob *job_params,
                                         bool *stop,
                                         bool *do_update,
                                         float *progress);
static void filelist_readjob_all_asset_library(FileListReadJob *job_params,
                                               bool *stop,
                                               bool *do_update,
                                               float *progress);

/* helper, could probably go in BKE actually? */
static int groupname_to_code(const char *group);

void filelist_setindexer(FileList *filelist, const FileIndexerType *indexer)
{
  BLI_assert(filelist);
  BLI_assert(indexer);

  filelist->indexer = indexer;
}

void filelist_set_asset_catalog_filter_options(
    FileList *filelist,
    eFileSel_Params_AssetCatalogVisibility catalog_visibility,
    const ::bUUID *catalog_id)
{
  if (!filelist->filter_data.asset_catalog_filter) {
    /* There's no filter data yet. */
    filelist->filter_data.asset_catalog_filter =
        blender::ed::asset_browser::file_create_asset_catalog_filter_settings();
  }

  const bool needs_update = file_set_asset_catalog_filter_settings(
      filelist->filter_data.asset_catalog_filter, catalog_visibility, *catalog_id);

  if (needs_update) {
    filelist_tag_needs_filtering(filelist);
  }
}

/**
 * Checks two libraries for equality.
 * \return True if the libraries match.
 */
static bool filelist_compare_asset_libraries(const AssetLibraryReference *library_a,
                                             const AssetLibraryReference *library_b)
{
  if (library_a->type != library_b->type) {
    return false;
  }
  if (library_a->type == ASSET_LIBRARY_CUSTOM) {
    /* Don't only check the index, also check that it's valid. */
    bUserAssetLibrary *library_ptr_a = BKE_preferences_asset_library_find_index(
        &U, library_a->custom_library_index);
    return (library_ptr_a != nullptr) &&
           (library_a->custom_library_index == library_b->custom_library_index);
  }

  return true;
}

void filelist_setlibrary(FileList *filelist, const AssetLibraryReference *asset_library_ref)
{
  /* Unset if needed. */
  if (!asset_library_ref) {
    if (filelist->asset_library_ref) {
      MEM_SAFE_FREE(filelist->asset_library_ref);
      filelist->flags |= FL_FORCE_RESET;
    }
    return;
  }

  if (!filelist->asset_library_ref) {
    filelist->asset_library_ref = MEM_callocN<AssetLibraryReference>("filelist asset library");
    *filelist->asset_library_ref = *asset_library_ref;

    filelist->flags |= FL_FORCE_RESET;
  }
  else if (!filelist_compare_asset_libraries(filelist->asset_library_ref, asset_library_ref)) {
    *filelist->asset_library_ref = *asset_library_ref;
    filelist->flags |= FL_FORCE_RESET;
  }
}

/* ********** Icon/image helpers ********** */

void filelist_free_icons()
{
  BLI_assert(G.background == false);

  for (int i = 0; i < int(SpecialFileImages::_Max); i++) {
    IMB_freeImBuf(gSpecialFileImages[i]);
    gSpecialFileImages[i] = nullptr;
  }
}

void filelist_file_get_full_path(const FileList *filelist,
                                 const FileDirEntry *file,
                                 char r_filepath[FILE_MAX_LIBEXTRA])
{
  if (file->asset) {
    const std::string asset_path = file->asset->full_path();
    BLI_strncpy(r_filepath, asset_path.c_str(), FILE_MAX_LIBEXTRA);
    return;
  }

  const char *root = filelist_dir(filelist);
  BLI_path_join(r_filepath, FILE_MAX_LIBEXTRA, root, file->relpath);
}

bool filelist_file_is_preview_pending(const FileList *filelist, const FileDirEntry *file)
{
  /* Actual preview loading is only started after the filelist is loaded, so the file isn't flagged
   * with #FILE_ENTRY_PREVIEW_LOADING yet. */
  const bool filelist_ready = filelist_is_ready(filelist);
  return !filelist_ready || file->flags & FILE_ENTRY_PREVIEW_LOADING;
}

static FileDirEntry *filelist_geticon_get_file(FileList *filelist, const int index)
{
  BLI_assert(G.background == false);

  return filelist_file(filelist, index);
}

ImBuf *filelist_get_preview_image(FileList *filelist, const int index)
{
  FileDirEntry *file = filelist_geticon_get_file(filelist, index);

  return file->preview_icon_id ? BKE_icon_imbuf_get_buffer(file->preview_icon_id) : nullptr;
}

ImBuf *filelist_file_get_preview_image(const FileDirEntry *file)
{
  return file->preview_icon_id ? BKE_icon_imbuf_get_buffer(file->preview_icon_id) : nullptr;
}

static ImBuf *filelist_ensure_special_file_image(SpecialFileImages image, int icon)
{
  ImBuf *ibuf = gSpecialFileImages[int(image)];
  if (ibuf) {
    return ibuf;
  }
  return gSpecialFileImages[int(image)] = UI_svg_icon_bitmap(icon, 256.0f, false);
}

ImBuf *filelist_geticon_special_file_image_ex(const FileDirEntry *file)
{
  ImBuf *ibuf = nullptr;

  if (file->typeflag & FILE_TYPE_DIR) {
    if (FILENAME_IS_PARENT(file->relpath)) {
      ibuf = filelist_ensure_special_file_image(SpecialFileImages::Parent, ICON_FILE_PARENT_LARGE);
    }
    else {
      ibuf = filelist_ensure_special_file_image(SpecialFileImages::Folder, ICON_FILE_FOLDER_LARGE);
    }
  }
  else {
    ibuf = filelist_ensure_special_file_image(SpecialFileImages::Document, ICON_FILE_LARGE);
  }

  return ibuf;
}

ImBuf *filelist_geticon_special_file_image(FileList *filelist, const int index)
{
  FileDirEntry *file = filelist_geticon_get_file(filelist, index);
  return filelist_geticon_special_file_image_ex(file);
}

static int filelist_geticon_file_type_ex(const FileList *filelist,
                                         const FileDirEntry *file,
                                         const bool is_main,
                                         const bool ignore_libdir)
{
  const eFileSel_File_Types typeflag = (eFileSel_File_Types)file->typeflag;

  if ((typeflag & FILE_TYPE_DIR) &&
      !(ignore_libdir && (typeflag & (FILE_TYPE_BLENDERLIB | FILE_TYPE_BLENDER))))
  {
    if (FILENAME_IS_PARENT(file->relpath)) {
      return is_main ? ICON_FILE_PARENT : ICON_NONE;
    }
    if (typeflag & FILE_TYPE_BUNDLE) {
      return ICON_UGLYPACKAGE;
    }
    if (typeflag & FILE_TYPE_BLENDER) {
      return ICON_FILE_BLEND;
    }
    if (is_main) {
      /* Do not return icon for folders if icons are not 'main' draw type
       * (e.g. when used over previews). */
      return (file->attributes & FILE_ATTR_ANY_LINK) ? ICON_FOLDER_REDIRECT : ICON_FILE_FOLDER;
    }

    /* If this path is in System list or path cache then use that icon. */
    FSMenu *fsmenu = ED_fsmenu_get();
    FSMenuCategory categories[] = {
        FS_CATEGORY_SYSTEM,
        FS_CATEGORY_SYSTEM_BOOKMARKS,
        FS_CATEGORY_OTHER,
    };

    for (int i = 0; i < ARRAY_SIZE(categories); i++) {
      FSMenuEntry *tfsm = ED_fsmenu_get_category(fsmenu, categories[i]);
      char fullpath[FILE_MAX_LIBEXTRA];
      char *target = fullpath;
      if (file->redirection_path) {
        target = file->redirection_path;
      }
      else if (filelist) {
        filelist_file_get_full_path(filelist, file, fullpath);
        BLI_path_slash_ensure(fullpath, sizeof(fullpath));
      }
      for (; tfsm; tfsm = tfsm->next) {
        if (STREQ(tfsm->path, target)) {
          /* Never want a little folder inside a large one. */
          return (tfsm->icon == ICON_FILE_FOLDER) ? ICON_NONE : tfsm->icon;
        }
      }
    }

    if (file->attributes & FILE_ATTR_OFFLINE) {
      return ICON_ERROR;
    }
    if (file->attributes & FILE_ATTR_TEMPORARY) {
      return ICON_FILE_CACHE;
    }
    if (file->attributes & FILE_ATTR_SYSTEM) {
      return ICON_SYSTEM;
    }
  }

  if (typeflag & FILE_TYPE_BLENDER) {
    return (is_main || file->preview_icon_id) ? ICON_FILE_BLEND : ICON_BLENDER;
  }
  if (typeflag & FILE_TYPE_BLENDER_BACKUP) {
    return ICON_FILE_BACKUP;
  }
  if (typeflag & FILE_TYPE_IMAGE) {
    return ICON_FILE_IMAGE;
  }
  if (typeflag & FILE_TYPE_MOVIE) {
    return ICON_FILE_MOVIE;
  }
  if (typeflag & FILE_TYPE_PYSCRIPT) {
    return ICON_FILE_SCRIPT;
  }
  if (typeflag & FILE_TYPE_SOUND) {
    return ICON_FILE_SOUND;
  }
  if (typeflag & FILE_TYPE_FTFONT) {
    return ICON_FILE_FONT;
  }
  if (typeflag & FILE_TYPE_BTX) {
    return ICON_FILE_BLANK;
  }
  if (typeflag & FILE_TYPE_ALEMBIC) {
    return ICON_FILE_3D;
  }
  if (typeflag & FILE_TYPE_USD) {
    return ICON_FILE_3D;
  }
  if (typeflag & FILE_TYPE_VOLUME) {
    return ICON_FILE_VOLUME;
  }
  if (typeflag & FILE_TYPE_OBJECT_IO) {
    return ICON_FILE_3D;
  }
  if (typeflag & FILE_TYPE_TEXT) {
    return ICON_FILE_TEXT;
  }
  if (typeflag & FILE_TYPE_ARCHIVE) {
    return ICON_FILE_ARCHIVE;
  }
  if (typeflag & FILE_TYPE_BLENDERLIB) {
    const int ret = UI_icon_from_idcode(file->blentype);
    if (ret != ICON_NONE) {
      return ret;
    }
  }
  return is_main ? ICON_FILE_BLANK : ICON_NONE;
}

int filelist_geticon_file_type(FileList *filelist, const int index, const bool is_main)
{
  FileDirEntry *file = filelist_geticon_get_file(filelist, index);

  return filelist_geticon_file_type_ex(filelist, file, is_main, false);
}

int ED_file_icon(const FileDirEntry *file)
{
  return file->preview_icon_id ? file->preview_icon_id :
                                 filelist_geticon_file_type_ex(nullptr, file, false, false);
}

bool filelist_intern_entry_is_main_file(const FileListInternEntry *intern_entry)
{
  return intern_entry->local_data.id != nullptr;
}

/* ********** Main ********** */

static void parent_dir_until_exists_or_default_root(char *dir)
{
  /* Only allow absolute paths as CWD relative doesn't make sense from the UI. */
  if (BLI_path_is_abs_from_cwd(dir) && BLI_path_parent_dir_until_exists(dir)) {
    return;
  }

#ifdef WIN32
  BLI_windows_get_default_root_dir(dir);
#else
  ARRAY_SET_ITEMS(dir, '/', '\0');
#endif
}

static bool filelist_checkdir_dir(const FileList * /*filelist*/,
                                  char dirpath[FILE_MAX_LIBEXTRA],
                                  const bool do_change)
{
  bool is_valid;
  if (do_change) {
    parent_dir_until_exists_or_default_root(dirpath);
    is_valid = true;
  }
  else {
    is_valid = BLI_path_is_abs_from_cwd(dirpath) && BLI_is_dir(dirpath);
  }
  return is_valid;
}

static bool filelist_checkdir_lib(const FileList * /*filelist*/,
                                  char dirpath[FILE_MAX_LIBEXTRA],
                                  const bool do_change)
{
  char tdir[FILE_MAX_LIBEXTRA];
  char *name;

  const bool is_valid = (BLI_is_dir(dirpath) ||
                         (BKE_blendfile_library_path_explode(dirpath, tdir, nullptr, &name) &&
                          BLI_is_file(tdir) && !name));

  if (do_change && !is_valid) {
    /* if not a valid library, we need it to be a valid directory! */
    parent_dir_until_exists_or_default_root(dirpath);
    return true;
  }
  return is_valid;
}

static bool filelist_checkdir_main(const FileList *filelist,
                                   char dirpath[FILE_MAX_LIBEXTRA],
                                   const bool do_change)
{
  /* TODO */
  return filelist_checkdir_lib(filelist, dirpath, do_change);
}

static bool filelist_checkdir_return_always_valid(const FileList * /*filelist*/,
                                                  char /*dirpath*/[FILE_MAX_LIBEXTRA],
                                                  const bool /*do_change*/)
{
  return true;
}

static void filelist_entry_clear(FileDirEntry *entry)
{
  if (entry->name && ((entry->flags & FILE_ENTRY_NAME_FREE) != 0)) {
    MEM_freeN((char *)entry->name);
  }
  if (entry->relpath) {
    MEM_freeN(entry->relpath);
  }
  if (entry->redirection_path) {
    MEM_freeN(entry->redirection_path);
  }
  if (entry->preview_icon_id) {
    BKE_icon_delete(entry->preview_icon_id);
    entry->preview_icon_id = 0;
  }
}

static void filelist_entry_free(FileDirEntry *entry)
{
  filelist_entry_clear(entry);
  MEM_freeN(entry);
}

static void filelist_direntryarr_free(FileDirEntryArr *array)
{
#if 0
  FileDirEntry *entry, *entry_next;

  for (entry = array->entries.first; entry; entry = entry_next) {
    entry_next = entry->next;
    filelist_entry_free(entry);
  }
  BLI_listbase_clear(&array->entries);
#else
  BLI_assert(BLI_listbase_is_empty(&array->entries));
#endif
  array->entries_num = FILEDIR_NBR_ENTRIES_UNSET;
  array->entries_filtered_num = FILEDIR_NBR_ENTRIES_UNSET;
}

static void filelist_intern_entry_free(FileList *filelist, FileListInternEntry *entry)
{
  if (auto asset_ptr = entry->asset.lock()) {
    BLI_assert(filelist->asset_library);
    filelist->asset_library->remove_asset(*asset_ptr);
  }

  if (entry->relpath) {
    MEM_freeN(entry->relpath);
  }
  if (entry->redirection_path) {
    MEM_freeN(entry->redirection_path);
  }
  if (entry->name && entry->free_name) {
    MEM_freeN((char *)entry->name);
  }
  MEM_delete(entry);
}

static void filelist_intern_free(FileList *filelist)
{
  FileListIntern *filelist_intern = &filelist->filelist_intern;
  LISTBASE_FOREACH_MUTABLE (FileListInternEntry *, entry, &filelist_intern->entries) {
    filelist_intern_entry_free(filelist, entry);
  }
  BLI_listbase_clear(&filelist_intern->entries);

  MEM_SAFE_FREE(filelist_intern->filtered);
}

/**
 * \return the number of main files removed.
 */
static int filelist_intern_free_main_files(FileList *filelist)
{
  FileListIntern *filelist_intern = &filelist->filelist_intern;
  int removed_counter = 0;
  LISTBASE_FOREACH_MUTABLE (FileListInternEntry *, entry, &filelist_intern->entries) {
    if (!filelist_intern_entry_is_main_file(entry)) {
      continue;
    }

    BLI_remlink(&filelist_intern->entries, entry);
    filelist_intern_entry_free(filelist, entry);
    removed_counter++;
  }

  if (removed_counter > 0) {
    MEM_SAFE_FREE(filelist_intern->filtered);
  }
  return removed_counter;
}

static void filelist_cache_preview_runf(TaskPool *__restrict pool, void *taskdata)
{
  FileListEntryCache *cache = static_cast<FileListEntryCache *>(BLI_task_pool_user_data(pool));
  FileListEntryPreviewTaskData *preview_taskdata = static_cast<FileListEntryPreviewTaskData *>(
      taskdata);
  FileListEntryPreview *preview = preview_taskdata->preview;

  /* XXX #THB_SOURCE_IMAGE for "historic" reasons. The case of an undefined source should be
   * handled better. */
  ThumbSource source = THB_SOURCE_IMAGE;

  //  printf("%s: Start (%d)...\n", __func__, threadid);

  //  printf("%s: %d - %s - %p\n", __func__, preview->index, preview->path, preview->img);
  BLI_assert(preview->flags &
             (FILE_TYPE_IMAGE | FILE_TYPE_MOVIE | FILE_TYPE_FTFONT | FILE_TYPE_BLENDER |
              FILE_TYPE_OBJECT_IO | FILE_TYPE_BLENDER_BACKUP | FILE_TYPE_BLENDERLIB));

  if (preview->flags & FILE_TYPE_IMAGE) {
    source = THB_SOURCE_IMAGE;
  }
  else if (preview->flags & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP | FILE_TYPE_BLENDERLIB))
  {
    source = THB_SOURCE_BLEND;
  }
  else if (preview->flags & FILE_TYPE_MOVIE) {
    source = THB_SOURCE_MOVIE;
  }
  else if (preview->flags & FILE_TYPE_FTFONT) {
    source = THB_SOURCE_FONT;
  }
  else if (preview->flags & FILE_TYPE_OBJECT_IO) {
    source = THB_SOURCE_OBJECT_IO;
  }

  IMB_thumb_path_lock(preview->filepath);
  /* Always generate biggest preview size for now, it's simpler and avoids having to re-generate
   * in case user switch to a bigger preview size. */
  ImBuf *imbuf = IMB_thumb_manage(preview->filepath, THB_LARGE, source);
  IMB_thumb_path_unlock(preview->filepath);
  if (imbuf) {
    preview->icon_id = BKE_icon_imbuf_create(imbuf);
  }

  /* Move ownership to the done queue. */
  preview_taskdata->preview = nullptr;

  BLI_thread_queue_push(cache->previews_done, preview, BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL);

  //  printf("%s: End (%d)...\n", __func__, threadid);
}

static void filelist_cache_preview_freef(TaskPool *__restrict /*pool*/, void *taskdata)
{
  FileListEntryPreviewTaskData *preview_taskdata = static_cast<FileListEntryPreviewTaskData *>(
      taskdata);

  /* In case the preview wasn't moved to the "done" queue yet. */
  if (preview_taskdata->preview) {
    MEM_freeN(preview_taskdata->preview);
  }

  MEM_freeN(preview_taskdata);
}

static void filelist_cache_preview_ensure_running(FileListEntryCache *cache)
{
  if (!cache->previews_pool) {
    cache->previews_pool = BLI_task_pool_create_background(cache, TASK_PRIORITY_LOW);
    cache->previews_done = BLI_thread_queue_init();
    cache->previews_todo_count = 0;

    IMB_thumb_locks_acquire();
  }
}

static void filelist_cache_previews_clear(FileListEntryCache *cache)
{
  if (cache->previews_pool) {
    BLI_task_pool_cancel(cache->previews_pool);

    LISTBASE_FOREACH (FileDirEntry *, entry, &cache->cached_entries) {
      entry->flags &= ~FILE_ENTRY_PREVIEW_LOADING;
    }

    FileListEntryPreview *preview;
    while ((preview = static_cast<FileListEntryPreview *>(
                BLI_thread_queue_pop_timeout(cache->previews_done, 0))))
    {
      // printf("%s: DONE %d - %s - %p\n", __func__, preview->index, preview->path,
      // preview->img);
      if (preview->icon_id) {
        BKE_icon_delete(preview->icon_id);
      }
      MEM_freeN(preview);
    }
    cache->previews_todo_count = 0;
  }
}

static void filelist_cache_previews_free(FileListEntryCache *cache)
{
  if (cache->previews_pool) {
    BLI_thread_queue_nowait(cache->previews_done);

    filelist_cache_previews_clear(cache);

    BLI_thread_queue_free(cache->previews_done);
    BLI_task_pool_free(cache->previews_pool);
    cache->previews_pool = nullptr;
    cache->previews_done = nullptr;
    cache->previews_todo_count = 0;

    IMB_thumb_locks_release();
  }

  cache->flags &= ~FLC_PREVIEWS_ACTIVE;
}

/**
 * Check if a preview for \a entry may be requested. Further conditions may apply, this just helps
 * to skip plenty of entries where it's easy to tell that no valid preview will be available or is
 * being loaded already.
 */
static bool filelist_file_preview_load_poll(const FileDirEntry *entry)
{
  if (entry->flags & (FILE_ENTRY_INVALID_PREVIEW | FILE_ENTRY_PREVIEW_LOADING)) {
    return false;
  }

  if (!(entry->typeflag &
        (FILE_TYPE_IMAGE | FILE_TYPE_MOVIE | FILE_TYPE_FTFONT | FILE_TYPE_OBJECT_IO |
         FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP | FILE_TYPE_BLENDERLIB)))
  {
    return false;
  }

  /* If we know this is an external ID without a preview, skip loading the preview. Can save quite
   * some time in heavy files, because otherwise for each missing preview and for each preview
   * reload, we'd reopen the .blend to look for the preview. */
  if ((entry->typeflag & FILE_TYPE_BLENDERLIB) &&
      (entry->flags & FILE_ENTRY_BLENDERLIB_NO_PREVIEW))
  {
    return false;
  }

  /* External ID that is also a directory is never previewed. */
  if ((entry->typeflag & (FILE_TYPE_BLENDERLIB | FILE_TYPE_DIR)) ==
      (FILE_TYPE_BLENDERLIB | FILE_TYPE_DIR))
  {
    return false;
  }

  return true;
}

/**
 * \return True if a new preview request was pushed, false otherwise (e.g. because the preview is
 * already loaded, invalid or not supported).
 */
static bool filelist_cache_previews_push(FileList *filelist, FileDirEntry *entry, const int index)
{
  FileListEntryCache *cache = filelist->filelist_cache;

  BLI_assert(cache->flags & FLC_PREVIEWS_ACTIVE);

  if (entry->preview_icon_id) {
    return false;
  }

  if (!filelist_file_preview_load_poll(entry)) {
    return false;
  }

  FileListInternEntry *intern_entry = filelist->filelist_intern.filtered[index];
  const PreviewImage *preview_in_memory = intern_entry->local_data.preview_image;
  if (preview_in_memory && !BKE_previewimg_is_finished(preview_in_memory, ICON_SIZE_PREVIEW)) {
    /* Nothing to set yet. Wait for next call. */
    return false;
  }

  filelist_cache_preview_ensure_running(cache);
  entry->flags |= FILE_ENTRY_PREVIEW_LOADING;

  FileListEntryPreview *preview = MEM_callocN<FileListEntryPreview>(__func__);
  preview->index = index;
  preview->flags = entry->typeflag;
  preview->icon_id = 0;

  if (preview_in_memory) {
    /* TODO(mano-wii): No need to use the thread API here. */
    BLI_assert(BKE_previewimg_is_finished(preview_in_memory, ICON_SIZE_PREVIEW));
    preview->filepath[0] = '\0';
    ImBuf *imbuf = BKE_previewimg_to_imbuf(preview_in_memory, ICON_SIZE_PREVIEW);
    if (imbuf) {
      preview->icon_id = BKE_icon_imbuf_create(imbuf);
    }
    BLI_thread_queue_push(cache->previews_done, preview, BLI_THREAD_QUEUE_WORK_PRIORITY_NORMAL);
  }
  else {
    if (entry->redirection_path) {
      BLI_strncpy(preview->filepath, entry->redirection_path, FILE_MAXDIR);
    }
    else {
      filelist_file_get_full_path(filelist, entry, preview->filepath);
    }
    // printf("%s: %d - %s\n", __func__, preview->index, preview->filepath);

    FileListEntryPreviewTaskData *preview_taskdata = MEM_callocN<FileListEntryPreviewTaskData>(
        __func__);
    preview_taskdata->preview = preview;
    BLI_task_pool_push(cache->previews_pool,
                       filelist_cache_preview_runf,
                       preview_taskdata,
                       true,
                       filelist_cache_preview_freef);
  }
  cache->previews_todo_count++;

  return true;
}

FileListEntryCache::FileListEntryCache() : size(FILELIST_ENTRYCACHESIZE_DEFAULT)
{
  block_entries = static_cast<FileDirEntry **>(
      MEM_mallocN(sizeof(*this->block_entries) * this->size, __func__));

  this->misc_entries = BLI_ghash_ptr_new_ex(__func__, this->size);
  this->misc_entries_indices = MEM_malloc_arrayN<int>(this->size, __func__);
  copy_vn_i(this->misc_entries_indices, this->size, -1);

  this->uids = BLI_ghash_new_ex(
      BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__, this->size * 2);
}

FileListEntryCache::~FileListEntryCache()
{
  filelist_cache_previews_free(this);

  MEM_freeN(this->block_entries);

  BLI_ghash_free(this->misc_entries, nullptr, nullptr);
  MEM_freeN(this->misc_entries_indices);

  BLI_ghash_free(this->uids, nullptr, nullptr);

  LISTBASE_FOREACH_MUTABLE (FileDirEntry *, entry, &this->cached_entries) {
    filelist_entry_free(entry);
  }
}

void filelist_cache_clear(FileListEntryCache *cache, size_t new_size)
{
  filelist_cache_previews_clear(cache);

  cache->block_cursor = cache->block_start_index = cache->block_center_index =
      cache->block_end_index = 0;
  if (new_size != cache->size) {
    cache->block_entries = static_cast<FileDirEntry **>(
        MEM_reallocN(cache->block_entries, sizeof(*cache->block_entries) * new_size));
  }

  BLI_ghash_clear_ex(cache->misc_entries, nullptr, nullptr, new_size);
  if (new_size != cache->size) {
    cache->misc_entries_indices = static_cast<int *>(MEM_reallocN(
        cache->misc_entries_indices, sizeof(*cache->misc_entries_indices) * new_size));
  }
  copy_vn_i(cache->misc_entries_indices, new_size, -1);

  BLI_ghash_clear_ex(cache->uids, nullptr, nullptr, new_size * 2);

  cache->size = new_size;

  LISTBASE_FOREACH_MUTABLE (FileDirEntry *, entry, &cache->cached_entries) {
    filelist_entry_free(entry);
  }
  BLI_listbase_clear(&cache->cached_entries);
}

FileList *filelist_new(short type)
{
  FileList *p = MEM_callocN<FileList>(__func__);

  p->filelist_cache = MEM_new<FileListEntryCache>("FileListEntryCache");

  p->selection_state = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
  p->filelist.entries_num = FILEDIR_NBR_ENTRIES_UNSET;
  filelist_settype(p, type);

  return p;
}

void filelist_settype(FileList *filelist, short type)
{
  if (filelist->type == type) {
    return;
  }

  filelist->type = (eFileSelectType)type;
  filelist->tags = 0;
  filelist->indexer = &file_indexer_noop;
  switch (filelist->type) {
    case FILE_MAIN:
      filelist->check_dir_fn = filelist_checkdir_main;
      filelist->read_job_fn = filelist_readjob_main;
      filelist->prepare_filter_fn = nullptr;
      filelist->filter_fn = is_filtered_main;
      break;
    case FILE_LOADLIB:
      filelist->check_dir_fn = filelist_checkdir_lib;
      filelist->read_job_fn = filelist_readjob_lib;
      filelist->prepare_filter_fn = nullptr;
      filelist->filter_fn = is_filtered_lib;
      break;
    case FILE_ASSET_LIBRARY:
      filelist->check_dir_fn = filelist_checkdir_lib;
      filelist->read_job_fn = filelist_readjob_asset_library;
      filelist->prepare_filter_fn = prepare_filter_asset_library;
      filelist->filter_fn = is_filtered_asset_library;
      filelist->tags |= FILELIST_TAGS_USES_MAIN_DATA;
      break;
    case FILE_MAIN_ASSET:
      filelist->check_dir_fn = filelist_checkdir_return_always_valid;
      filelist->read_job_fn = filelist_readjob_main_assets;
      filelist->prepare_filter_fn = prepare_filter_asset_library;
      filelist->filter_fn = is_filtered_main_assets;
      filelist->tags |= FILELIST_TAGS_USES_MAIN_DATA | FILELIST_TAGS_NO_THREADS;
      break;
    case FILE_ASSET_LIBRARY_ALL:
      filelist->check_dir_fn = filelist_checkdir_return_always_valid;
      filelist->read_job_fn = filelist_readjob_all_asset_library;
      filelist->prepare_filter_fn = prepare_filter_asset_library;
      filelist->filter_fn = is_filtered_asset_library;
      filelist->tags |= FILELIST_TAGS_USES_MAIN_DATA;
      break;
    default:
      filelist->check_dir_fn = filelist_checkdir_dir;
      filelist->read_job_fn = filelist_readjob_dir;
      filelist->prepare_filter_fn = nullptr;
      filelist->filter_fn = is_filtered_file;
      break;
  }

  filelist->flags |= FL_FORCE_RESET;
}

static void filelist_clear_asset_library(FileList *filelist)
{
  /* The AssetLibraryService owns the AssetLibrary pointer, so no need for us to free it. */
  filelist->asset_library = nullptr;
  file_delete_asset_catalog_filter_settings(&filelist->filter_data.asset_catalog_filter);
}

void filelist_clear_ex(FileList *filelist,
                       const bool do_asset_library,
                       const bool do_cache,
                       const bool do_selection)
{
  if (!filelist) {
    return;
  }

  filelist_tag_needs_filtering(filelist);

  if (do_cache) {
    filelist_cache_clear(filelist->filelist_cache, filelist->filelist_cache->size);
  }

  filelist_intern_free(filelist);

  filelist_direntryarr_free(&filelist->filelist);

  if (do_selection && filelist->selection_state) {
    BLI_ghash_clear(filelist->selection_state, nullptr, nullptr);
  }

  if (do_asset_library) {
    filelist_clear_asset_library(filelist);
  }
}

static void filelist_clear_main_files(FileList *filelist,
                                      const bool do_asset_library,
                                      const bool do_cache,
                                      const bool do_selection)
{
  if (!filelist || !(filelist->tags & FILELIST_TAGS_USES_MAIN_DATA)) {
    return;
  }
  if (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET) {
    return;
  }
  const int removed_files = filelist_intern_free_main_files(filelist);
  /* File list contains no main files to clear. */
  if (removed_files == 0) {
    return;
  }

  filelist_tag_needs_filtering(filelist);

  if (do_cache) {
    filelist_cache_clear(filelist->filelist_cache, filelist->filelist_cache->size);
  }

  filelist->filelist.entries_num -= removed_files;
  filelist->filelist.entries_filtered_num = FILEDIR_NBR_ENTRIES_UNSET;
  BLI_assert(filelist->filelist.entries_num > FILEDIR_NBR_ENTRIES_UNSET);

  if (do_selection && filelist->selection_state) {
    BLI_ghash_clear(filelist->selection_state, nullptr, nullptr);
  }

  if (do_asset_library) {
    filelist_clear_asset_library(filelist);
  }
}

void filelist_clear(FileList *filelist)
{
  filelist_clear_ex(filelist, true, true, true);
}

void filelist_clear_from_reset_tag(FileList *filelist)
{
  /* Do a full clear if needed. */
  if (filelist->flags & FL_FORCE_RESET) {
    filelist_clear(filelist);
    return;
  }

  if (filelist->flags & FL_FORCE_RESET_MAIN_FILES) {
    filelist_clear_main_files(filelist, false, true, false);
    return;
  }
}

void filelist_free(FileList *filelist)
{
  if (!filelist) {
    printf("Attempting to delete empty filelist.\n");
    return;
  }

  /* No need to clear cache & selection_state, we free them anyway. */
  filelist_clear_ex(filelist, true, false, false);
  MEM_delete(filelist->filelist_cache);

  if (filelist->selection_state) {
    BLI_ghash_free(filelist->selection_state, nullptr, nullptr);
    filelist->selection_state = nullptr;
  }

  MEM_SAFE_FREE(filelist->asset_library_ref);

  memset(&filelist->filter_data, 0, sizeof(filelist->filter_data));

  filelist->flags &= ~(FL_NEED_SORTING | FL_NEED_FILTERING);

  MEM_freeN(filelist);
}

blender::asset_system::AssetLibrary *filelist_asset_library(FileList *filelist)
{
  return filelist->asset_library;
}

void filelist_freelib(FileList *filelist)
{
  if (filelist->libfiledata) {
    BLO_blendhandle_close(filelist->libfiledata);
  }
  filelist->libfiledata = nullptr;
}

BlendHandle *filelist_lib(FileList *filelist)
{
  return filelist->libfiledata;
}

int filelist_files_num_entries(FileList *filelist)
{
  return filelist->filelist.entries_num;
}

static const char *fileentry_uiname(const char *root, FileListInternEntry *entry, char *buff)
{
  if (asset_system::AssetRepresentation *asset = entry->get_asset()) {
    const StringRefNull asset_name = asset->get_name();
    return BLI_strdupn(asset_name.c_str(), asset_name.size());
  }

  const char *relpath = entry->relpath;
  const eFileSel_File_Types typeflag = entry->typeflag;
  char *name = nullptr;

  if (typeflag & FILE_TYPE_FTFONT && !(typeflag & FILE_TYPE_BLENDERLIB)) {
    char abspath[FILE_MAX_LIBEXTRA];
    BLI_path_join(abspath, sizeof(abspath), root, relpath);
    name = BLF_display_name_from_file(abspath);
    if (name) {
      /* Allocated string, so no need to #BLI_strdup. */
      return name;
    }
  }

  if (typeflag & FILE_TYPE_BLENDERLIB) {
    char abspath[FILE_MAX_LIBEXTRA];
    char *group;

    BLI_path_join(abspath, sizeof(abspath), root, relpath);
    BKE_blendfile_library_path_explode(abspath, buff, &group, &name);
    if (!name) {
      name = group;
    }
  }
  /* Depending on platforms, 'my_file.blend/..' might be viewed as dir or not... */
  if (!name) {
    if (typeflag & FILE_TYPE_DIR) {
      name = (char *)relpath;
    }
    else {
      name = (char *)BLI_path_basename(relpath);
    }
  }
  BLI_assert(name);

  return BLI_strdup(name);
}

const char *filelist_dir(const FileList *filelist)
{
  return filelist->filelist.root;
}

bool filelist_is_dir(const FileList *filelist, const char *path)
{
  return filelist->check_dir_fn(filelist, (char *)path, false);
}

void filelist_setdir(FileList *filelist, char dirpath[FILE_MAX_LIBEXTRA])
{
  const bool allow_invalid = filelist->asset_library_ref != nullptr;
  BLI_assert(strlen(dirpath) < FILE_MAX_LIBEXTRA);

  BLI_path_abs(dirpath, BKE_main_blendfile_path_from_global());
  BLI_path_normalize_dir(dirpath, FILE_MAX_LIBEXTRA);
  const bool is_valid_path = filelist->check_dir_fn(filelist, dirpath, !allow_invalid);
  BLI_assert(is_valid_path || allow_invalid);
  UNUSED_VARS_NDEBUG(is_valid_path);

  if (!STREQ(filelist->filelist.root, dirpath)) {
    STRNCPY(filelist->filelist.root, dirpath);
    filelist->flags |= FL_FORCE_RESET;
  }
}

void filelist_setrecursion(FileList *filelist, const int recursion_level)
{
  if (filelist->max_recursion != recursion_level) {
    filelist->max_recursion = recursion_level;
    filelist->flags |= FL_FORCE_RESET;
  }
}

bool filelist_needs_force_reset(const FileList *filelist)
{
  return (filelist->flags & (FL_FORCE_RESET | FL_FORCE_RESET_MAIN_FILES)) != 0;
}

void filelist_tag_force_reset(FileList *filelist)
{
  filelist->flags |= FL_FORCE_RESET;
}

void filelist_tag_force_reset_mainfiles(FileList *filelist)
{
  if (!(filelist->tags & FILELIST_TAGS_USES_MAIN_DATA)) {
    return;
  }
  filelist->flags |= FL_FORCE_RESET_MAIN_FILES;
}

void filelist_tag_reload_asset_library(FileList *filelist)
{
  filelist->flags |= FL_RELOAD_ASSET_LIBRARY;
}

bool filelist_is_ready(const FileList *filelist)
{
  return (filelist->flags & FL_IS_READY) != 0;
}

bool filelist_pending(const FileList *filelist)
{
  return (filelist->flags & FL_IS_PENDING) != 0;
}

bool filelist_needs_reset_on_main_changes(const FileList *filelist)
{
  return (filelist->tags & FILELIST_TAGS_USES_MAIN_DATA) != 0;
}

int filelist_files_ensure(FileList *filelist)
{
  if (!filelist_needs_force_reset(filelist) || !filelist_needs_reading(filelist)) {
    filelist_sort(filelist);
    filelist_filter(filelist);
  }

  return filelist->filelist.entries_filtered_num;
}

static FileDirEntry *filelist_file_create_entry(FileList *filelist, const int index)
{
  FileListInternEntry *entry = filelist->filelist_intern.filtered[index];
  FileListEntryCache *cache = filelist->filelist_cache;
  FileDirEntry *ret;

  ret = MEM_callocN<FileDirEntry>(__func__);

  ret->size = uint64_t(entry->st.st_size);
  ret->time = int64_t(entry->st.st_mtime);

  ret->relpath = BLI_strdup(entry->relpath);
  if (entry->free_name) {
    ret->name = BLI_strdup(entry->name);
    ret->flags |= FILE_ENTRY_NAME_FREE;
  }
  else {
    ret->name = entry->name;
  }
  ret->uid = entry->uid;
  ret->blentype = entry->blentype;
  ret->typeflag = entry->typeflag;
  ret->attributes = entry->attributes;
  if (entry->redirection_path) {
    ret->redirection_path = BLI_strdup(entry->redirection_path);
  }
  ret->id = entry->local_data.id;
  ret->asset = entry->get_asset();
  /* For some file types the preview is already available. */
  if (entry->local_data.preview_image &&
      BKE_previewimg_is_finished(entry->local_data.preview_image, ICON_SIZE_PREVIEW))
  {
    ImBuf *ibuf = BKE_previewimg_to_imbuf(entry->local_data.preview_image, ICON_SIZE_PREVIEW);
    if (ibuf) {
      ret->preview_icon_id = BKE_icon_imbuf_create(ibuf);
    }
  }
  if (entry->blenderlib_has_no_preview) {
    ret->flags |= FILE_ENTRY_BLENDERLIB_NO_PREVIEW;
  }
  BLI_addtail(&cache->cached_entries, ret);
  return ret;
}

static void filelist_file_release_entry(FileList *filelist, FileDirEntry *entry)
{
  BLI_remlink(&filelist->filelist_cache->cached_entries, entry);
  filelist_entry_free(entry);
}

static FileDirEntry *filelist_cache_file_lookup(FileListEntryCache *cache, const int index)
{
  /* If the file is cached, we can get it from either the block or the misc entry storage. */

  if (index >= cache->block_start_index && index < cache->block_end_index) {
    const int idx = (index - cache->block_start_index + cache->block_cursor) % cache->size;
    return cache->block_entries[idx];
  }

  return static_cast<FileDirEntry *>(
      BLI_ghash_lookup(cache->misc_entries, POINTER_FROM_INT(index)));
}

FileDirEntry *filelist_file_ex(FileList *filelist, const int index, const bool use_request)
{
  FileDirEntry *ret = nullptr, *old;
  FileListEntryCache *cache = filelist->filelist_cache;
  int old_index;

  if ((index < 0) || (index >= filelist->filelist.entries_filtered_num)) {
    return ret;
  }

  if ((ret = filelist_cache_file_lookup(cache, index))) {
    return ret;
  }

  if (!use_request) {
    return nullptr;
  }

  //  printf("requesting file %d (not yet cached)\n", index);

  /* Else, we have to add new entry to 'misc' cache - and possibly make room for it first! */
  ret = filelist_file_create_entry(filelist, index);
  old_index = cache->misc_entries_indices[cache->misc_cursor];
  if ((old = static_cast<FileDirEntry *>(
           BLI_ghash_popkey(cache->misc_entries, POINTER_FROM_INT(old_index), nullptr))))
  {
    BLI_ghash_remove(cache->uids, POINTER_FROM_UINT(old->uid), nullptr, nullptr);
    filelist_file_release_entry(filelist, old);
  }
  BLI_ghash_insert(cache->misc_entries, POINTER_FROM_INT(index), ret);
  BLI_ghash_insert(cache->uids, POINTER_FROM_UINT(ret->uid), ret);

  cache->misc_entries_indices[cache->misc_cursor] = index;
  cache->misc_cursor = (cache->misc_cursor + 1) % cache->size;

#if 0 /* Actually no, only block cached entries should have preview IMHO. */
  if (cache->previews_pool) {
    filelist_cache_previews_push(filelist, ret, index);
  }
#endif

  return ret;
}

FileDirEntry *filelist_file(FileList *filelist, int index)
{
  return filelist_file_ex(filelist, index, true);
}

int filelist_file_find_path(FileList *filelist, const char *filename)
{
  if (filelist->filelist.entries_filtered_num == FILEDIR_NBR_ENTRIES_UNSET) {
    return -1;
  }

  /* XXX TODO: Cache could probably use a ghash on paths too? Not really urgent though.
   * This is only used to find again renamed entry,
   * annoying but looks hairy to get rid of it currently. */

  for (int fidx = 0; fidx < filelist->filelist.entries_filtered_num; fidx++) {
    FileListInternEntry *entry = filelist->filelist_intern.filtered[fidx];
    if (STREQ(entry->relpath, filename)) {
      return fidx;
    }
  }

  return -1;
}

int filelist_file_find_id(const FileList *filelist, const ID *id)
{
  if (filelist->filelist.entries_filtered_num == FILEDIR_NBR_ENTRIES_UNSET) {
    return -1;
  }

  for (int fidx = 0; fidx < filelist->filelist.entries_filtered_num; fidx++) {
    FileListInternEntry *entry = filelist->filelist_intern.filtered[fidx];
    if (entry->local_data.id == id) {
      return fidx;
    }
  }

  return -1;
}

static FileListInternEntry *filelist_entry_intern_get(const FileList *filelist, const int index)
{
  BLI_assert(index >= 0 && index < filelist->filelist.entries_filtered_num);
  return filelist->filelist_intern.filtered[index];
}

ID *filelist_entry_get_id(const FileList *filelist, const int index)
{
  const FileListInternEntry *intern_entry = filelist_entry_intern_get(filelist, index);
  return intern_entry->local_data.id;
}

asset_system::AssetRepresentation *filelist_entry_get_asset_representation(
    const FileList *filelist, const int index)
{
  const FileListInternEntry *intern_entry = filelist_entry_intern_get(filelist, index);
  return intern_entry->get_asset();
}

ID *filelist_file_get_id(const FileDirEntry *file)
{
  return file->id;
}

const char *filelist_entry_get_relpath(const FileList *filelist, int index)
{
  const FileListInternEntry *intern_entry = filelist_entry_intern_get(filelist, index);
  return intern_entry->relpath;
}

#define FILE_UID_UNSET 0

static FileUID filelist_uid_generate(FileList *filelist)
{
  /* Using an atomic operation to avoid having to lock thread...
   * Note that we do not really need this here currently, since there is a single listing thread,
   * but better remain consistent about threading! */
  return atomic_add_and_fetch_uint32(&filelist->filelist_intern.curr_uid, 1);
}

bool filelist_uid_is_set(const FileUID uid)
{
  FileUID unset_uid;
  filelist_uid_unset(&unset_uid);
  return unset_uid != uid;
}

void filelist_uid_unset(FileUID *r_uid)
{
  *r_uid = FILE_UID_UNSET;
}

void filelist_file_cache_slidingwindow_set(FileList *filelist, size_t window_size)
{
  /* Always keep it power of 2, in [256, 8192] range for now,
   * cache being app. twice bigger than requested window. */
  size_t size = 256;
  window_size *= 2;

  while (size < window_size && size < 8192) {
    size *= 2;
  }

  if (size != filelist->filelist_cache->size) {
    filelist_cache_clear(filelist->filelist_cache, size);
  }
}

/* Helpers, low-level, they assume cursor + size <= cache_size */
static bool filelist_file_cache_block_create(FileList *filelist,
                                             const int start_index,
                                             const int size,
                                             int cursor)
{
  FileListEntryCache *cache = filelist->filelist_cache;

  int i, idx;

  for (i = 0, idx = start_index; i < size; i++, idx++, cursor++) {
    FileDirEntry *entry;

    /* That entry might have already been requested and stored in misc cache... */
    if ((entry = static_cast<FileDirEntry *>(
             BLI_ghash_popkey(cache->misc_entries, POINTER_FROM_INT(idx), nullptr))) == nullptr)
    {
      entry = filelist_file_create_entry(filelist, idx);
      BLI_ghash_insert(cache->uids, POINTER_FROM_UINT(entry->uid), entry);
    }
    cache->block_entries[cursor] = entry;
  }
  return true;
}

static void filelist_file_cache_block_release(FileList *filelist, const int size, int cursor)
{
  FileListEntryCache *cache = filelist->filelist_cache;

  int i;

  for (i = 0; i < size; i++, cursor++) {
    FileDirEntry *entry = cache->block_entries[cursor];
#if 0
    printf("%s: release cacheidx %d (%%p %%s)\n",
           __func__,
           cursor /*, cache->block_entries[cursor], cache->block_entries[cursor]->relpath*/);
#endif
    BLI_ghash_remove(cache->uids, POINTER_FROM_UINT(entry->uid), nullptr, nullptr);
    filelist_file_release_entry(filelist, entry);
#ifndef NDEBUG
    cache->block_entries[cursor] = nullptr;
#endif
  }
}

bool filelist_file_cache_block(FileList *filelist, const int index)
{
  FileListEntryCache *cache = filelist->filelist_cache;
  const size_t cache_size = cache->size;

  const int entries_num = filelist->filelist.entries_filtered_num;
  int start_index = max_ii(0, index - (cache_size / 2));
  int end_index = min_ii(entries_num, index + (cache_size / 2));
  int i;
  const bool full_refresh = (filelist->flags & FL_IS_READY) == 0;

  if ((index < 0) || (index >= entries_num)) {
    //      printf("Wrong index %d ([%d:%d])", index, 0, entries_num);
    return false;
  }

  /* Maximize cached range! */
  if ((end_index - start_index) < cache_size) {
    if (start_index == 0) {
      end_index = min_ii(entries_num, start_index + cache_size);
    }
    else if (end_index == entries_num) {
      start_index = max_ii(0, end_index - cache_size);
    }
  }

  BLI_assert((end_index - start_index) <= cache_size);

  //  printf("%s: [%d:%d] around index %d (current cache: [%d:%d])\n", __func__,
  //         start_index, end_index, index, cache->block_start_index, cache->block_end_index);

  /* If we have something to (re)cache... */
  if (full_refresh || (start_index != cache->block_start_index) ||
      (end_index != cache->block_end_index))
  {
    if (full_refresh || (start_index >= cache->block_end_index) ||
        (end_index <= cache->block_start_index))
    {
      int size1 = cache->block_end_index - cache->block_start_index;
      int size2 = 0;
      int idx1 = cache->block_cursor, idx2 = 0;

      //          printf("Full Recaching!\n");

      if (cache->flags & FLC_PREVIEWS_ACTIVE) {
        filelist_cache_previews_clear(cache);
      }

      if (idx1 + size1 > cache_size) {
        size2 = idx1 + size1 - cache_size;
        size1 -= size2;
        filelist_file_cache_block_release(filelist, size2, idx2);
      }
      filelist_file_cache_block_release(filelist, size1, idx1);

      cache->block_start_index = cache->block_end_index = cache->block_cursor = 0;

      /* New cached block does not overlap existing one, simple. */
      if (!filelist_file_cache_block_create(filelist, start_index, end_index - start_index, 0)) {
        return false;
      }

      cache->block_start_index = start_index;
      cache->block_end_index = end_index;
    }
    else {
      //          printf("Partial Recaching!\n");

      /* At this point, we know we keep part of currently cached entries, so update previews
       * if needed, and remove everything from working queue - we'll add all newly needed
       * entries at the end. */
      if (cache->flags & FLC_PREVIEWS_ACTIVE) {
        filelist_cache_previews_update(filelist);
        filelist_cache_previews_clear(cache);
      }

      //          printf("\tpreview cleaned up...\n");

      if (start_index > cache->block_start_index) {
        int size1 = start_index - cache->block_start_index;
        int size2 = 0;
        int idx1 = cache->block_cursor, idx2 = 0;

        //              printf("\tcache releasing: [%d:%d] (%d, %d)\n",
        //                     cache->block_start_index, cache->block_start_index + size1,
        //                     cache->block_cursor, size1);

        if (idx1 + size1 > cache_size) {
          size2 = idx1 + size1 - cache_size;
          size1 -= size2;
          filelist_file_cache_block_release(filelist, size2, idx2);
        }
        filelist_file_cache_block_release(filelist, size1, idx1);

        cache->block_cursor = (idx1 + size1 + size2) % cache_size;
        cache->block_start_index = start_index;
      }
      if (end_index < cache->block_end_index) {
        int size1 = cache->block_end_index - end_index;
        int size2 = 0;
        int idx1, idx2 = 0;

#if 0
        printf("\tcache releasing: [%d:%d] (%d)\n",
               cache->block_end_index - size1,
               cache->block_end_index,
               cache->block_cursor);
#endif

        idx1 = (cache->block_cursor + end_index - cache->block_start_index) % cache_size;
        if (idx1 + size1 > cache_size) {
          size2 = idx1 + size1 - cache_size;
          size1 -= size2;
          filelist_file_cache_block_release(filelist, size2, idx2);
        }
        filelist_file_cache_block_release(filelist, size1, idx1);

        cache->block_end_index = end_index;
      }

      //          printf("\tcache cleaned up...\n");

      if (start_index < cache->block_start_index) {
        /* Add (request) needed entries before already cached ones. */
        /* NOTE: We need some index black magic to wrap around (cycle)
         * inside our cache_size array... */
        int size1 = cache->block_start_index - start_index;
        int size2 = 0;
        int idx1, idx2;

        if (size1 > cache->block_cursor) {
          size2 = size1;
          size1 -= cache->block_cursor;
          size2 -= size1;
          idx2 = 0;
          idx1 = cache_size - size1;
        }
        else {
          idx1 = cache->block_cursor - size1;
        }

        if (size2) {
          if (!filelist_file_cache_block_create(filelist, start_index + size1, size2, idx2)) {
            return false;
          }
        }
        if (!filelist_file_cache_block_create(filelist, start_index, size1, idx1)) {
          return false;
        }

        cache->block_cursor = idx1;
        cache->block_start_index = start_index;
      }
      //          printf("\tstart-extended...\n");
      if (end_index > cache->block_end_index) {
        /* Add (request) needed entries after already cached ones. */
        /* NOTE: We need some index black magic to wrap around (cycle)
         * inside our cache_size array... */
        int size1 = end_index - cache->block_end_index;
        int size2 = 0;
        int idx1, idx2;

        idx1 = (cache->block_cursor + end_index - cache->block_start_index - size1) % cache_size;
        if ((idx1 + size1) > cache_size) {
          size2 = size1;
          size1 = cache_size - idx1;
          size2 -= size1;
          idx2 = 0;
        }

        if (size2) {
          if (!filelist_file_cache_block_create(filelist, end_index - size2, size2, idx2)) {
            return false;
          }
        }
        if (!filelist_file_cache_block_create(filelist, end_index - size1 - size2, size1, idx1)) {
          return false;
        }

        cache->block_end_index = end_index;
      }

      //          printf("\tend-extended...\n");
    }
  }
  else if ((cache->block_center_index != index) && (cache->flags & FLC_PREVIEWS_ACTIVE)) {
    /* We try to always preview visible entries first, so 'restart' preview background task. */
    filelist_cache_previews_update(filelist);
    filelist_cache_previews_clear(cache);
  }

  //  printf("Re-queueing previews...\n");

  if (cache->flags & FLC_PREVIEWS_ACTIVE) {
    /* Note we try to preview first images around given index - i.e. assumed visible ones. */
    int block_index = cache->block_cursor + (index - start_index);
    int offs_max = max_ii(end_index - index, index - start_index);
    for (i = 0; i <= offs_max; i++) {
      int offs = i;
      do {
        int offs_idx = index + offs;
        if (start_index <= offs_idx && offs_idx < end_index) {
          int offs_block_idx = (block_index + offs) % int(cache_size);
          filelist_cache_previews_push(filelist, cache->block_entries[offs_block_idx], offs_idx);
        }
      } while ((offs = -offs) < 0); /* Switch between negative and positive offset. */
    }
  }

  cache->block_center_index = index;

  //  printf("%s Finished!\n", __func__);

  return true;
}

void filelist_cache_previews_set(FileList *filelist, const bool use_previews)
{
  FileListEntryCache *cache = filelist->filelist_cache;

  if (use_previews == ((cache->flags & FLC_PREVIEWS_ACTIVE) != 0)) {
    return;
  }
  /* Do not start preview work while listing, gives nasty flickering! */
  if (use_previews && (filelist->flags & FL_IS_READY)) {
    cache->flags |= FLC_PREVIEWS_ACTIVE;

    BLI_assert((cache->previews_pool == nullptr) && (cache->previews_done == nullptr) &&
               (cache->previews_todo_count == 0));

    //      printf("%s: Init Previews...\n", __func__);

    /* No need to populate preview queue here, filelist_file_cache_block() handles this. */
  }
  else {
    //      printf("%s: Clear Previews...\n", __func__);

    filelist_cache_previews_free(cache);
  }
}

bool filelist_cache_previews_update(FileList *filelist)
{
  FileListEntryCache *cache = filelist->filelist_cache;
  TaskPool *pool = cache->previews_pool;
  bool changed = false;

  if (!pool) {
    return changed;
  }

  //  printf("%s: Update Previews...\n", __func__);

  while (!BLI_thread_queue_is_empty(cache->previews_done)) {
    FileListEntryPreview *preview = static_cast<FileListEntryPreview *>(
        BLI_thread_queue_pop(cache->previews_done));
    FileDirEntry *entry;

    /* Paranoid (should never happen currently
     * since we consume this queue from a single thread), but... */
    if (!preview) {
      continue;
    }
    /* entry might have been removed from cache in the mean time,
     * we do not want to cache it again here. */
    entry = filelist_file_ex(filelist, preview->index, false);

    // printf("%s: %d - %s - %p\n", __func__, preview->index, preview->filepath, preview->img);

    if (entry) {
      if (preview->icon_id) {
        /* The FILE_ENTRY_PREVIEW_LOADING flag should have prevented any other asynchronous
         * process from trying to generate the same preview icon. */
        BLI_assert_msg(!entry->preview_icon_id, "Preview icon should not have been generated yet");

        /* Move ownership over icon. */
        entry->preview_icon_id = preview->icon_id;
        preview->icon_id = 0;
      }
      else {
        /* We want to avoid re-processing this entry continuously!
         * Note that, since entries only live in cache,
         * preview will be retried quite often anyway. */
        entry->flags |= FILE_ENTRY_INVALID_PREVIEW;
      }
      entry->flags &= ~FILE_ENTRY_PREVIEW_LOADING;
      changed = true;
    }
    else {
      BKE_icon_delete(preview->icon_id);
    }

    MEM_freeN(preview);
    cache->previews_todo_count--;
  }

  return changed;
}

bool filelist_cache_previews_running(FileList *filelist)
{
  FileListEntryCache *cache = filelist->filelist_cache;

  return (cache->previews_pool != nullptr);
}

bool filelist_cache_previews_done(FileList *filelist)
{
  FileListEntryCache *cache = filelist->filelist_cache;
  if ((cache->flags & FLC_PREVIEWS_ACTIVE) == 0) {
    /* There are no previews. */
    return false;
  }

  return (cache->previews_pool == nullptr) || (cache->previews_done == nullptr) ||
         (cache->previews_todo_count == 0);
}

/* would recognize .blend as well */
static bool file_is_blend_backup(const char *str)
{
  const size_t a = strlen(str);
  size_t b = 7;
  bool retval = false;

  if (a == 0 || b >= a) {
    /* pass */
  }
  else {
    const char *loc;

    if (a > b + 1) {
      b++;
    }

    /* allow .blend1 .blend2 .blend32 */
    loc = BLI_strcasestr(str + a - b, ".blend");

    if (loc) {
      retval = true;
    }
  }

  return retval;
}

int ED_path_extension_type(const char *path)
{
  /* ATTENTION: Never return OR'ed bit-flags here, always return a single enum value! Some code
   * using this may do `ELEM()`-like checks. */

  if (BKE_blendfile_extension_check(path)) {
    return FILE_TYPE_BLENDER;
  }
  if (file_is_blend_backup(path)) {
    return FILE_TYPE_BLENDER_BACKUP;
  }
#ifdef __APPLE__
  if (BLI_path_extension_check_n(path,
                                 /* Application bundle */
                                 ".app",
                                 /* Safari in-progress/paused download */
                                 ".download",
                                 nullptr))
  {
    return FILE_TYPE_BUNDLE;
  }
#endif
  if (BLI_path_extension_check(path, ".py")) {
    return FILE_TYPE_PYSCRIPT;
  }
  if (BLI_path_extension_check_n(path,
                                 ".txt",
                                 ".glsl",
                                 ".osl",
                                 ".data",
                                 ".pov",
                                 ".ini",
                                 ".mcr",
                                 ".inc",
                                 ".fountain",
                                 nullptr))
  {
    return FILE_TYPE_TEXT;
  }

  /* NOTE: While `.ttc` & `.otc` files can be loaded, only a single "face" is supported,
   * users will have to extract bold/italic etc manually for Blender to use them, see #44254. */
  if (BLI_path_extension_check_n(path, ".ttf", ".pfb", ".otf", ".woff", ".woff2", nullptr)) {
    return FILE_TYPE_FTFONT;
  }
  if (BLI_path_extension_check(path, ".btx")) {
    return FILE_TYPE_BTX;
  }
  if (BLI_path_extension_check(path, ".abc")) {
    return FILE_TYPE_ALEMBIC;
  }
  if (BLI_path_extension_check_n(path, ".usd", ".usda", ".usdc", ".usdz", nullptr)) {
    return FILE_TYPE_USD;
  }
  if (BLI_path_extension_check(path, ".vdb")) {
    return FILE_TYPE_VOLUME;
  }
  if (BLI_path_extension_check(path, ".zip")) {
    return FILE_TYPE_ARCHIVE;
  }
  if (BLI_path_extension_check_n(
          path, ".obj", ".mtl", ".3ds", ".fbx", ".glb", ".gltf", ".svg", ".ply", ".stl", nullptr))
  {
    return FILE_TYPE_OBJECT_IO;
  }
  if (BLI_path_extension_check_array(path, imb_ext_image)) {
    return FILE_TYPE_IMAGE;
  }
  if (BLI_path_extension_check(path, ".ogg")) {
    if (MOV_is_movie_file(path)) {
      return FILE_TYPE_MOVIE;
    }
    return FILE_TYPE_SOUND;
  }
  if (BLI_path_extension_check_array(path, imb_ext_movie)) {
    return FILE_TYPE_MOVIE;
  }
  if (BLI_path_extension_check_array(path, imb_ext_audio)) {
    return FILE_TYPE_SOUND;
  }
  return 0;
}

int ED_file_extension_icon(const char *path)
{
  const int type = ED_path_extension_type(path);

  switch (type) {
    case FILE_TYPE_BLENDER:
      return ICON_FILE_BLEND;
    case FILE_TYPE_BLENDER_BACKUP:
      return ICON_FILE_BACKUP;
    case FILE_TYPE_IMAGE:
      return ICON_FILE_IMAGE;
    case FILE_TYPE_MOVIE:
      return ICON_FILE_MOVIE;
    case FILE_TYPE_PYSCRIPT:
      return ICON_FILE_SCRIPT;
    case FILE_TYPE_SOUND:
      return ICON_FILE_SOUND;
    case FILE_TYPE_FTFONT:
      return ICON_FILE_FONT;
    case FILE_TYPE_BTX:
      return ICON_FILE_BLANK;
    case FILE_TYPE_ALEMBIC:
    case FILE_TYPE_OBJECT_IO:
      return ICON_FILE_3D;
    case FILE_TYPE_TEXT:
      return ICON_FILE_TEXT;
    case FILE_TYPE_ARCHIVE:
      return ICON_FILE_ARCHIVE;
    case FILE_TYPE_VOLUME:
      return ICON_FILE_VOLUME;
    default:
      return ICON_FILE_BLANK;
  }
}

int filelist_needs_reading(const FileList *filelist)
{
  return (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET) ||
         filelist_needs_force_reset(filelist);
}

uint filelist_entry_select_set(const FileList *filelist,
                               const FileDirEntry *entry,
                               FileSelType select,
                               const eDirEntry_SelectFlag flag,
                               FileCheckType check)
{
  /* Default nullptr pointer if not found is fine here! */
  void **es_p = BLI_ghash_lookup_p(filelist->selection_state, POINTER_FROM_UINT(entry->uid));
  eDirEntry_SelectFlag entry_flag = eDirEntry_SelectFlag(es_p ? POINTER_AS_UINT(*es_p) : 0);
  const eDirEntry_SelectFlag org_entry_flag = entry_flag;

  BLI_assert(entry);
  BLI_assert(ELEM(check, CHECK_DIRS, CHECK_FILES, CHECK_ALL));

  if ((check == CHECK_ALL) || ((check == CHECK_DIRS) && (entry->typeflag & FILE_TYPE_DIR)) ||
      ((check == CHECK_FILES) && !(entry->typeflag & FILE_TYPE_DIR)))
  {
    switch (select) {
      case FILE_SEL_REMOVE:
        entry_flag &= ~flag;
        break;
      case FILE_SEL_ADD:
        entry_flag |= flag;
        break;
      case FILE_SEL_TOGGLE:
        entry_flag ^= flag;
        break;
    }
  }

  if (entry_flag != org_entry_flag) {
    if (es_p) {
      if (entry_flag) {
        *es_p = POINTER_FROM_UINT(entry_flag);
      }
      else {
        BLI_ghash_remove(
            filelist->selection_state, POINTER_FROM_UINT(entry->uid), nullptr, nullptr);
      }
    }
    else if (entry_flag) {
      BLI_ghash_insert(
          filelist->selection_state, POINTER_FROM_UINT(entry->uid), POINTER_FROM_UINT(entry_flag));
    }
  }

  return entry_flag;
}

void filelist_entry_select_index_set(FileList *filelist,
                                     const int index,
                                     FileSelType select,
                                     const eDirEntry_SelectFlag flag,
                                     FileCheckType check)
{
  FileDirEntry *entry = filelist_file(filelist, index);

  if (entry) {
    filelist_entry_select_set(filelist, entry, select, flag, check);
  }
}

void filelist_entries_select_index_range_set(FileList *filelist,
                                             FileSelection *sel,
                                             FileSelType select,
                                             const eDirEntry_SelectFlag flag,
                                             FileCheckType check)
{
  /* select all valid files between first and last indicated */
  if ((sel->first >= 0) && (sel->first < filelist->filelist.entries_filtered_num) &&
      (sel->last >= 0) && (sel->last < filelist->filelist.entries_filtered_num))
  {
    int current_file;
    for (current_file = sel->first; current_file <= sel->last; current_file++) {
      filelist_entry_select_index_set(filelist, current_file, select, flag, check);
    }
  }
}

eDirEntry_SelectFlag filelist_entry_select_get(FileList *filelist,
                                               FileDirEntry *entry,
                                               FileCheckType check)
{
  BLI_assert(entry);
  BLI_assert(ELEM(check, CHECK_DIRS, CHECK_FILES, CHECK_ALL));

  if ((check == CHECK_ALL) || ((check == CHECK_DIRS) && (entry->typeflag & FILE_TYPE_DIR)) ||
      ((check == CHECK_FILES) && !(entry->typeflag & FILE_TYPE_DIR)))
  {
    /* Default nullptr pointer if not found is fine here! */
    return eDirEntry_SelectFlag(POINTER_AS_UINT(
        BLI_ghash_lookup(filelist->selection_state, POINTER_FROM_UINT(entry->uid))));
  }

  return eDirEntry_SelectFlag(0);
}

eDirEntry_SelectFlag filelist_entry_select_index_get(FileList *filelist,
                                                     const int index,
                                                     FileCheckType check)
{
  FileDirEntry *entry = filelist_file(filelist, index);

  if (entry) {
    return filelist_entry_select_get(filelist, entry, check);
  }

  return eDirEntry_SelectFlag(0);
}

bool filelist_entry_is_selected(FileList *filelist, const int index)
{
  BLI_assert(index >= 0 && index < filelist->filelist.entries_filtered_num);
  FileListInternEntry *intern_entry = filelist->filelist_intern.filtered[index];

  /* BLI_ghash_lookup returns nullptr if not found, which gets mapped to 0, which gets mapped to
   * "not selected". */
  const eDirEntry_SelectFlag selection_state = eDirEntry_SelectFlag(POINTER_AS_UINT(
      BLI_ghash_lookup(filelist->selection_state, POINTER_FROM_UINT(intern_entry->uid))));

  return selection_state != 0;
}

void filelist_entry_parent_select_set(FileList *filelist,
                                      FileSelType select,
                                      const eDirEntry_SelectFlag flag,
                                      FileCheckType check)
{
  if ((filelist->filter_data.flags & FLF_HIDE_PARENT) == 0) {
    filelist_entry_select_index_set(filelist, 0, select, flag, check);
  }
}

bool filelist_islibrary(FileList *filelist, char *dir, char **r_group)
{
  if (filelist->asset_library) {
    return true;
  }
  return BKE_blendfile_library_path_explode(filelist->filelist.root, dir, r_group, nullptr);
}

static int groupname_to_code(const char *group)
{
  char buf[BLO_GROUP_MAX];
  char *lslash;

  BLI_assert(group);

  STRNCPY(buf, group);
  lslash = (char *)BLI_path_slash_rfind(buf);
  if (lslash) {
    lslash[0] = '\0';
  }

  return buf[0] ? BKE_idtype_idcode_from_name(buf) : 0;
}

/**
 * From here, we are in 'Job Context',
 * i.e. have to be careful about sharing stuff between background working thread.
 * and main one (used by UI among other things).
 */
struct TodoDir {
  int level;
  char *dir;
};

struct FileListReadJob {
  blender::Mutex lock;
  char main_filepath[FILE_MAX] = "";
  Main *current_main = nullptr;
  FileList *filelist = nullptr;

  /**
   * The path currently being read, relative to the filelist root directory.
   * Needed for recursive reading. The full file path is then composed like:
   * `<filelist root>/<cur_relbase>/<file name>`.
   * (whereby the file name may also be a library path within a .blend, e.g.
   * `Materials/Material.001`).
   */
  char cur_relbase[FILE_MAX_LIBEXTRA] = "";

  /** The current asset library to load. Usually the same as #FileList.asset_library, however
   * sometimes the #FileList one is a combination of multiple other ones ("All" asset library),
   * which need to be loaded individually. Then this can be set to override the #FileList library.
   * Use this in all loading code. */
  asset_system::AssetLibrary *load_asset_library = nullptr;
  /** Set to request a partial read that only adds files representing #Main data (IDs). Used when
   * #Main may have received changes of interest (e.g. asset removed or renamed). */
  bool only_main_data = false;
  /** Trigger a call to #AS_asset_library_load() to update asset catalogs (won't reload the actual
   * assets) */
  bool reload_asset_library = false;

  /** Shallow copy of #filelist for thread-safe access.
   *
   * The job system calls #filelist_readjob_update which moves any read file from #tmp_filelist
   * into #filelist in a thread-safe way.
   *
   * #tmp_filelist also keeps an `AssetLibrary *` so that it can be loaded in the same thread,
   * and moved to #filelist once all categories are loaded.
   *
   * NOTE: #tmp_filelist is freed in #filelist_readjob_free, so any copied pointers need to be
   * set to nullptr to avoid double-freeing them. */
  FileList *tmp_filelist = nullptr;
};

/**
 * Append \a filename (or even a path inside of a .blend, like `Material/Material.001`), to the
 * current relative path being read within the filelist root. The returned string needs freeing
 * with #MEM_freeN().
 */
static char *current_relpath_append(const FileListReadJob *job_params, const char *filename)
{
  const char *relbase = job_params->cur_relbase;

  /* Early exit, nothing to join. */
  if (!relbase[0]) {
    return BLI_strdup(filename);
  }

  BLI_assert(ELEM(relbase[strlen(relbase) - 1], SEP, ALTSEP));
  BLI_assert(BLI_path_is_rel(relbase));

  char relpath[FILE_MAX_LIBEXTRA];
  /* Using #BLI_path_join works but isn't needed as `rel_subdir` has a trailing slash. */
  BLI_string_join(relpath,
                  sizeof(relpath),
                  /* + 2 to remove "//" relative path prefix. */
                  relbase + 2,
                  filename);

  return BLI_strdup(relpath);
}

static int filelist_readjob_list_dir(FileListReadJob *job_params,
                                     const char *root,
                                     ListBase *entries,
                                     const char *filter_glob,
                                     const bool do_lib,
                                     const char *main_filepath,
                                     const bool skip_currpar)
{
  direntry *files;
  int entries_num = 0;
  /* Full path of the item. */
  char full_path[FILE_MAX];

  const int files_num = BLI_filelist_dir_contents(root, &files);
  if (files) {
    int i = files_num;
    while (i--) {
      FileListInternEntry *entry;

      if (skip_currpar && FILENAME_IS_CURRPAR(files[i].relname)) {
        continue;
      }

      entry = MEM_new<FileListInternEntry>(__func__);
      entry->relpath = current_relpath_append(job_params, files[i].relname);
      entry->st = files[i].s;

      BLI_path_join(full_path, FILE_MAX, root, files[i].relname);
      char *target = full_path;

      /* Set initial file type and attributes. */
      entry->attributes = BLI_file_attributes(full_path);
      if (S_ISDIR(files[i].s.st_mode)
#ifdef __APPLE__
          && !(ED_path_extension_type(full_path) & FILE_TYPE_BUNDLE)
#endif
      )
      {
        entry->typeflag = FILE_TYPE_DIR;
      }

      /* Is this a file that points to another file? */
      if (entry->attributes & FILE_ATTR_ALIAS) {
        entry->redirection_path = MEM_calloc_arrayN<char>(FILE_MAXDIR, __func__);
        if (BLI_file_alias_target(full_path, entry->redirection_path)) {
          if (BLI_is_dir(entry->redirection_path)) {
            entry->typeflag = FILE_TYPE_DIR;
            BLI_path_slash_ensure(entry->redirection_path, FILE_MAXDIR);
          }
          else {
            entry->typeflag = (eFileSel_File_Types)ED_path_extension_type(entry->redirection_path);
          }
          target = entry->redirection_path;
#ifdef WIN32
          /* On Windows don't show `.lnk` extension for valid shortcuts. */
          BLI_path_extension_strip(entry->relpath);
#endif
        }
        else {
          MEM_freeN(entry->redirection_path);
          entry->redirection_path = nullptr;
          entry->attributes |= FILE_ATTR_HIDDEN;
        }
      }

      if (!(entry->typeflag & FILE_TYPE_DIR)) {
        if (do_lib && BKE_blendfile_extension_check(target)) {
          /* If we are considering .blend files as libraries, promote them to directory status. */
          entry->typeflag = FILE_TYPE_BLENDER;
          /* prevent current file being used as acceptable dir */
          if (BLI_path_cmp(main_filepath, target) != 0) {
            entry->typeflag |= FILE_TYPE_DIR;
          }
        }
        else {
          entry->typeflag = (eFileSel_File_Types)ED_path_extension_type(target);
          if (filter_glob[0] && BLI_path_extension_check_glob(target, filter_glob)) {
            entry->typeflag |= FILE_TYPE_OPERATOR;
          }
        }
      }

#ifndef WIN32
      /* Set linux-style dot files hidden too. */
      if (BLI_path_has_hidden_component(entry->relpath)) {
        entry->attributes |= FILE_ATTR_HIDDEN;
      }
#endif

      BLI_addtail(entries, entry);
      entries_num++;
    }
    BLI_filelist_free(files, files_num);
  }
  return entries_num;
}

enum ListLibOptions {
  LIST_LIB_OPTION_NONE = 0,

  /* Will read both the groups + actual ids from the library. Reduces the amount of times that
   * a library needs to be opened. */
  LIST_LIB_RECURSIVE = (1 << 0),

  /* Will only list assets. */
  LIST_LIB_ASSETS_ONLY = (1 << 1),

  /* Add given root as result. */
  LIST_LIB_ADD_PARENT = (1 << 2),
};
ENUM_OPERATORS(ListLibOptions);

static FileListInternEntry *filelist_readjob_list_lib_group_create(
    const FileListReadJob *job_params, const int idcode, const char *group_name)
{
  FileListInternEntry *entry = MEM_new<FileListInternEntry>(__func__);
  entry->relpath = current_relpath_append(job_params, group_name);
  entry->typeflag |= FILE_TYPE_BLENDERLIB | FILE_TYPE_DIR;
  entry->blentype = idcode;
  return entry;
}

/**
 * \warning: This "steals" the asset metadata from \a datablock_info. Not great design but fixing
 *           this requires redesigning things on the caller side for proper ownership management.
 */
static void filelist_readjob_list_lib_add_datablock(FileListReadJob *job_params,
                                                    ListBase *entries,
                                                    BLODataBlockInfo *datablock_info,
                                                    const bool prefix_relpath_with_group_name,
                                                    const int idcode,
                                                    const char *group_name)
{
  FileListInternEntry *entry = MEM_new<FileListInternEntry>(__func__);
  if (prefix_relpath_with_group_name) {
    std::string datablock_path = StringRef(group_name) + SEP_STR + datablock_info->name;
    entry->relpath = current_relpath_append(job_params, datablock_path.c_str());
  }
  else {
    entry->relpath = current_relpath_append(job_params, datablock_info->name);
  }
  entry->typeflag |= FILE_TYPE_BLENDERLIB;
  if (datablock_info) {
    entry->blenderlib_has_no_preview = datablock_info->no_preview_found;

    if (datablock_info->name[0] == '.') {
      entry->attributes |= FILE_ATTR_HIDDEN;
    }

    if (datablock_info->asset_data) {
      entry->typeflag |= FILE_TYPE_ASSET;

      if (job_params->load_asset_library) {
        /* Take ownership over the asset data (shallow copies into unique_ptr managed memory) to
         * pass it on to the asset system. */
        std::unique_ptr metadata = std::make_unique<AssetMetaData>(
            std::move(*datablock_info->asset_data));
        MEM_delete(datablock_info->asset_data);
        /* Give back a non-owning pointer, because the data-block info is still needed (e.g. to
         * update the asset index). */
        datablock_info->asset_data = metadata.get();
        datablock_info->free_asset_data = false;

        entry->asset = job_params->load_asset_library->add_external_asset(
            entry->relpath, datablock_info->name, idcode, std::move(metadata));
      }
    }
  }
  entry->blentype = idcode;
  BLI_addtail(entries, entry);
}

static void filelist_readjob_list_lib_add_datablocks(FileListReadJob *job_params,
                                                     ListBase *entries,
                                                     LinkNode *datablock_infos,
                                                     const bool prefix_relpath_with_group_name,
                                                     const int idcode,
                                                     const char *group_name)
{
  for (LinkNode *ln = datablock_infos; ln; ln = ln->next) {
    BLODataBlockInfo *datablock_info = static_cast<BLODataBlockInfo *>(ln->link);
    filelist_readjob_list_lib_add_datablock(
        job_params, entries, datablock_info, prefix_relpath_with_group_name, idcode, group_name);
  }
}

static void filelist_readjob_list_lib_add_from_indexer_entries(
    FileListReadJob *job_params,
    ListBase *entries,
    const FileIndexerEntries *indexer_entries,
    const bool prefix_relpath_with_group_name)
{
  for (const LinkNode *ln = indexer_entries->entries; ln; ln = ln->next) {
    FileIndexerEntry *indexer_entry = static_cast<FileIndexerEntry *>(ln->link);
    const char *group_name = BKE_idtype_idcode_to_name(indexer_entry->idcode);
    filelist_readjob_list_lib_add_datablock(job_params,
                                            entries,
                                            &indexer_entry->datablock_info,
                                            prefix_relpath_with_group_name,
                                            indexer_entry->idcode,
                                            group_name);
  }
}

static FileListInternEntry *filelist_readjob_list_lib_navigate_to_parent_entry_create(
    const FileListReadJob *job_params)
{
  FileListInternEntry *entry = MEM_new<FileListInternEntry>(__func__);
  entry->relpath = current_relpath_append(job_params, FILENAME_PARENT);
  entry->typeflag |= (FILE_TYPE_BLENDERLIB | FILE_TYPE_DIR);
  return entry;
}

/**
 * Structure to keep the file indexer and its user data together.
 */
struct FileIndexer {
  const FileIndexerType *callbacks;

  /**
   * User data. Contains the result of `callbacks.init_user_data`.
   */
  void *user_data;
};

static int filelist_readjob_list_lib_populate_from_index(FileListReadJob *job_params,
                                                         ListBase *entries,
                                                         const ListLibOptions options,
                                                         const int read_from_index,
                                                         const FileIndexerEntries *indexer_entries)
{
  int navigate_to_parent_len = 0;
  if (options & LIST_LIB_ADD_PARENT) {
    FileListInternEntry *entry = filelist_readjob_list_lib_navigate_to_parent_entry_create(
        job_params);
    BLI_addtail(entries, entry);
    navigate_to_parent_len = 1;
  }

  filelist_readjob_list_lib_add_from_indexer_entries(job_params, entries, indexer_entries, true);
  return read_from_index + navigate_to_parent_len;
}

/**
 * \return The number of entries found if the \a root path points to a valid library file.
 *         Otherwise returns no value (#std::nullopt).
 */
static std::optional<int> filelist_readjob_list_lib(FileListReadJob *job_params,
                                                    const char *root,
                                                    ListBase *entries,
                                                    const ListLibOptions options,
                                                    FileIndexer *indexer_runtime)
{
  BLI_assert(indexer_runtime);

  char dir[FILE_MAX_LIBEXTRA], *group;

  BlendHandle *libfiledata = nullptr;

  /* Check if the given root is actually a library. All folders are passed to
   * `filelist_readjob_list_lib` and based on the number of found entries `filelist_readjob_do`
   * will do a dir listing only when this function does not return any entries. */
  /* TODO(jbakker): We should consider introducing its own function to detect if it is a lib and
   * call it directly from `filelist_readjob_do` to increase readability. */
  const bool is_lib = BKE_blendfile_library_path_explode(root, dir, &group, nullptr);
  if (!is_lib) {
    return std::nullopt;
  }

  /* The root path contains an ID group (e.g. "Materials" or "Objects"). */
  const bool has_group = group != nullptr;

  /* Try read from indexer_runtime. */
  /* Indexing returns all entries in a blend file. We should ignore the index when listing a group
   * inside a blend file, so the `entries` isn't filled with undesired entries.
   * This happens when linking or appending data-blocks, where you can navigate into a group (ie
   * Materials/Objects) where you only want to work with partial indexes.
   *
   * Adding support for partial reading/updating indexes would increase the complexity.
   */
  const bool use_indexer = !has_group;
  FileIndexerEntries indexer_entries = {nullptr};
  if (use_indexer) {
    int read_from_index = 0;
    eFileIndexerResult indexer_result = indexer_runtime->callbacks->read_index(
        dir, &indexer_entries, &read_from_index, indexer_runtime->user_data);
    if (indexer_result == FILE_INDEXER_ENTRIES_LOADED) {
      int entries_read = filelist_readjob_list_lib_populate_from_index(
          job_params, entries, options, read_from_index, &indexer_entries);
      ED_file_indexer_entries_clear(&indexer_entries);
      return entries_read;
    }
  }

  /* Open the library file. */
  BlendFileReadReport bf_reports{};
  libfiledata = BLO_blendhandle_from_file(dir, &bf_reports);
  if (libfiledata == nullptr) {
    return std::nullopt;
  }

  /* Add current parent when requested. */
  /* Is the navigate to previous level added to the list of entries. When added the return value
   * should be increased to match the actual number of entries added. It is introduced to keep
   * the code clean and readable and not counting in a single variable. */
  int navigate_to_parent_len = 0;
  if (options & LIST_LIB_ADD_PARENT) {
    FileListInternEntry *entry = filelist_readjob_list_lib_navigate_to_parent_entry_create(
        job_params);
    BLI_addtail(entries, entry);
    navigate_to_parent_len = 1;
  }

  int group_len = 0;
  int datablock_len = 0;
  /* Read only the datablocks from this group. */
  if (has_group) {
    const int idcode = groupname_to_code(group);
    LinkNode *datablock_infos = BLO_blendhandle_get_datablock_info(
        libfiledata, idcode, options & LIST_LIB_ASSETS_ONLY, &datablock_len);
    filelist_readjob_list_lib_add_datablocks(
        job_params, entries, datablock_infos, false, idcode, group);
    BLO_datablock_info_linklist_free(datablock_infos);
  }
  /* Read all datablocks from all groups. */
  else {
    LinkNode *groups = BLO_blendhandle_get_linkable_groups(libfiledata);
    group_len = BLI_linklist_count(groups);

    for (LinkNode *ln = groups; ln; ln = ln->next) {
      const char *group_name = static_cast<char *>(ln->link);
      const int idcode = groupname_to_code(group_name);
      FileListInternEntry *group_entry = filelist_readjob_list_lib_group_create(
          job_params, idcode, group_name);
      BLI_addtail(entries, group_entry);

      if (options & LIST_LIB_RECURSIVE) {
        int group_datablock_len;
        LinkNode *group_datablock_infos = BLO_blendhandle_get_datablock_info(
            libfiledata, idcode, options & LIST_LIB_ASSETS_ONLY, &group_datablock_len);
        filelist_readjob_list_lib_add_datablocks(
            job_params, entries, group_datablock_infos, true, idcode, group_name);
        if (use_indexer) {
          ED_file_indexer_entries_extend_from_datablock_infos(
              &indexer_entries, group_datablock_infos, idcode);
        }
        BLO_datablock_info_linklist_free(group_datablock_infos);
        datablock_len += group_datablock_len;
      }
    }

    BLI_linklist_freeN(groups);
  }

  BLO_blendhandle_close(libfiledata);

  /* Update the index. */
  if (use_indexer) {
    indexer_runtime->callbacks->update_index(dir, &indexer_entries, indexer_runtime->user_data);
    ED_file_indexer_entries_clear(&indexer_entries);
  }

  /* Return the number of items added to entries. */
  int added_entries_len = group_len + datablock_len + navigate_to_parent_len;
  return added_entries_len;
}

#if 0
/* Kept for reference here, in case we want to add back that feature later.
 * We do not need it currently. */
/* Code ***NOT*** updated for job stuff! */
static void filelist_readjob_main_recursive(Main *bmain, FileList *filelist)
{
  ID *id;
  FileDirEntry *files, *firstlib = nullptr;
  ListBase *lb;
  int a, fake, idcode, ok, totlib, totbl;

  // filelist->type = FILE_MAIN; /* XXX TODO: add modes to file-browser */

  BLI_assert(filelist->filelist.entries == nullptr);

  if (filelist->filelist.root[0] == '/') {
    filelist->filelist.root[0] = '\0';
  }

  if (filelist->filelist.root[0]) {
    idcode = groupname_to_code(filelist->filelist.root);
    if (idcode == 0) {
      filelist->filelist.root[0] = '\0';
    }
  }

  if (filelist->dir[0] == 0) {
    /* make directories */
#  ifdef WITH_FREESTYLE
    filelist->filelist.entries_num = 27;
#  else
    filelist->filelist.entries_num = 26;
#  endif
    filelist_resize(filelist, filelist->filelist.entries_num);

    for (a = 0; a < filelist->filelist.entries_num; a++) {
      filelist->filelist.entries[a].typeflag |= FILE_TYPE_DIR;
    }

    filelist->filelist.entries[0].entry->relpath = BLI_strdup(FILENAME_PARENT);
    filelist->filelist.entries[1].entry->relpath = BLI_strdup("Scene");
    filelist->filelist.entries[2].entry->relpath = BLI_strdup("Object");
    filelist->filelist.entries[3].entry->relpath = BLI_strdup("Mesh");
    filelist->filelist.entries[4].entry->relpath = BLI_strdup("Curve");
    filelist->filelist.entries[5].entry->relpath = BLI_strdup("Metaball");
    filelist->filelist.entries[6].entry->relpath = BLI_strdup("Material");
    filelist->filelist.entries[7].entry->relpath = BLI_strdup("Texture");
    filelist->filelist.entries[8].entry->relpath = BLI_strdup("Image");
    filelist->filelist.entries[9].entry->relpath = BLI_strdup("Ika");
    filelist->filelist.entries[10].entry->relpath = BLI_strdup("Wave");
    filelist->filelist.entries[11].entry->relpath = BLI_strdup("Lattice");
    filelist->filelist.entries[12].entry->relpath = BLI_strdup("Light");
    filelist->filelist.entries[13].entry->relpath = BLI_strdup("Camera");
    filelist->filelist.entries[14].entry->relpath = BLI_strdup("Ipo");
    filelist->filelist.entries[15].entry->relpath = BLI_strdup("World");
    filelist->filelist.entries[16].entry->relpath = BLI_strdup("Screen");
    filelist->filelist.entries[17].entry->relpath = BLI_strdup("VFont");
    filelist->filelist.entries[18].entry->relpath = BLI_strdup("Text");
    filelist->filelist.entries[19].entry->relpath = BLI_strdup("Armature");
    filelist->filelist.entries[20].entry->relpath = BLI_strdup("Action");
    filelist->filelist.entries[21].entry->relpath = BLI_strdup("NodeTree");
    filelist->filelist.entries[22].entry->relpath = BLI_strdup("Speaker");
    filelist->filelist.entries[23].entry->relpath = BLI_strdup("Curves");
    filelist->filelist.entries[24].entry->relpath = BLI_strdup("Point Cloud");
    filelist->filelist.entries[25].entry->relpath = BLI_strdup("Volume");
#  ifdef WITH_FREESTYLE
    filelist->filelist.entries[26].entry->relpath = BLI_strdup("FreestyleLineStyle");
#  endif
  }
  else {
    /* make files */
    idcode = groupname_to_code(filelist->filelist.root);

    lb = which_libbase(bmain, idcode);
    if (lb == nullptr) {
      return;
    }

    filelist->filelist.entries_num = 0;
    for (id = lb->first; id; id = id->next) {
      if (!(filelist->filter_data.flags & FLF_HIDE_DOT) || id->name[2] != '.') {
        filelist->filelist.entries_num++;
      }
    }

    /* XXX TODO: if data-browse or append/link #FLF_HIDE_PARENT has to be set. */
    if (!(filelist->filter_data.flags & FLF_HIDE_PARENT)) {
      filelist->filelist.entries_num++;
    }

    if (filelist->filelist.entries_num > 0) {
      filelist_resize(filelist, filelist->filelist.entries_num);
    }

    files = filelist->filelist.entries;

    if (!(filelist->filter_data.flags & FLF_HIDE_PARENT)) {
      files->entry->relpath = BLI_strdup(FILENAME_PARENT);
      files->typeflag |= FILE_TYPE_DIR;

      files++;
    }

    totlib = totbl = 0;
    for (id = lb->first; id; id = id->next) {
      ok = 1;
      if (ok) {
        if (!(filelist->filter_data.flags & FLF_HIDE_DOT) || id->name[2] != '.') {
          if (!ID_IS_LINKED(id)) {
            files->entry->relpath = BLI_strdup(id->name + 2);
          }
          else {
            char relname[FILE_MAX + (MAX_ID_NAME - 2) + 3];
            SNPRINTF(relname, "%s | %s", id->lib->filepath, id->name + 2);
            files->entry->relpath = BLI_strdup(relname);
          }
//                  files->type |= S_IFREG;
#  if 0 /* XXX TODO: show the selection status of the objects. */
          if (!filelist->has_func) { /* F4 DATA BROWSE */
            if (idcode == ID_OB) {
              if (((Object *)id)->flag & SELECT) {
                files->entry->selflag |= FILE_SEL_SELECTED;
              }
            }
            else if (idcode == ID_SCE) {
              if (((Scene *)id)->r.scemode & R_BG_RENDER) {
                files->entry->selflag |= FILE_SEL_SELECTED;
              }
            }
          }
#  endif
          //                  files->entry->nr = totbl + 1;
          files->entry->poin = id;
          fake = id->flag & ID_FLAG_FAKEUSER;
          if (ELEM(idcode, ID_MA, ID_TE, ID_LA, ID_WO, ID_IM)) {
            files->typeflag |= FILE_TYPE_IMAGE;
          }
#  if 0
          if (id->lib && fake) {
            SNPRINTF_UTF8(files->extra, "LF %d", id->us);
          }
          else if (id->lib) {
            SNPRINTF_UTF8(files->extra, "L    %d", id->us);
          }
          else if (fake) {
            SNPRINTF_UTF8(files->extra, "F    %d", id->us);
          }
          else {
            SNPRINTF_UTF8(files->extra, "      %d", id->us);
          }
#  endif

          if (id->lib) {
            if (totlib == 0) {
              firstlib = files;
            }
            totlib++;
          }

          files++;
        }
        totbl++;
      }
    }

    /* only qsort of library blocks */
    if (totlib > 1) {
      qsort(firstlib, totlib, sizeof(*files), compare_name);
    }
  }
}
#endif

/**
 * \return True if new entries were added to the file list.
 */
static bool filelist_readjob_append_entries(FileListReadJob *job_params,
                                            ListBase *from_entries,
                                            int from_entries_num)
{
  BLI_assert(BLI_listbase_count(from_entries) == from_entries_num);
  if (from_entries_num <= 0) {
    return false;
  }

  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  std::scoped_lock lock(job_params->lock);
  BLI_movelisttolist(&filelist->filelist.entries, from_entries);
  filelist->filelist.entries_num += from_entries_num;

  return true;
}

static bool filelist_readjob_should_recurse_into_entry(const int max_recursion,
                                                       const bool is_lib,
                                                       const int current_recursion_level,
                                                       FileListInternEntry *entry)
{
  if (max_recursion == 0) {
    /* Recursive loading is disabled. */
    return false;
  }
  if (!is_lib && current_recursion_level > max_recursion) {
    /* No more levels of recursion left. */
    return false;
  }
  /* Show entries when recursion is set to `Blend file` even when `current_recursion_level`
   * exceeds `max_recursion`. */
  if (!is_lib && (current_recursion_level >= max_recursion) &&
      ((entry->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) == 0))
  {
    return false;
  }
  if (entry->typeflag & FILE_TYPE_BLENDERLIB) {
    /* Libraries are already loaded recursively when recursive loaded is used. No need to add
     * them another time. This loading is done with the `LIST_LIB_RECURSIVE` option. */
    return false;
  }
  if (!(entry->typeflag & FILE_TYPE_DIR)) {
    /* Cannot recurse into regular file entries. */
    return false;
  }
  if (FILENAME_IS_CURRPAR(entry->relpath)) {
    /* Don't schedule go to parent entry, (`..`) */
    return false;
  }

  return true;
}

static void filelist_readjob_recursive_dir_add_items(const bool do_lib,
                                                     FileListReadJob *job_params,
                                                     const bool *stop,
                                                     bool *do_update,
                                                     float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  ListBase entries = {nullptr};
  BLI_Stack *todo_dirs;
  TodoDir *td_dir;
  char dir[FILE_MAX_LIBEXTRA];
  char filter_glob[FILE_MAXFILE];
  const char *root = filelist->filelist.root;
  const int max_recursion = filelist->max_recursion;
  int dirs_done_count = 0, dirs_todo_count = 1;

  todo_dirs = BLI_stack_new(sizeof(*td_dir), __func__);
  td_dir = static_cast<TodoDir *>(BLI_stack_push_r(todo_dirs));
  td_dir->level = 1;

  STRNCPY(dir, filelist->filelist.root);
  STRNCPY(filter_glob, filelist->filter_data.filter_glob);

  BLI_path_abs(dir, job_params->main_filepath);
  BLI_path_normalize_dir(dir, sizeof(dir));
  td_dir->dir = BLI_strdup(dir);

  /* Init the file indexer. */
  FileIndexer indexer_runtime{};
  indexer_runtime.callbacks = filelist->indexer;
  if (indexer_runtime.callbacks->init_user_data) {
    indexer_runtime.user_data = indexer_runtime.callbacks->init_user_data(dir, sizeof(dir));
  }

  while (!BLI_stack_is_empty(todo_dirs) && !(*stop)) {
    int entries_num = 0;

    char *subdir;
    char rel_subdir[FILE_MAX_LIBEXTRA];
    int recursion_level;
    bool skip_currpar;

    td_dir = static_cast<TodoDir *>(BLI_stack_peek(todo_dirs));
    subdir = td_dir->dir;
    recursion_level = td_dir->level;
    skip_currpar = (recursion_level > 1);

    BLI_stack_discard(todo_dirs);

    /* ARRRG! We have to be very careful *not to use* common `BLI_path_utils.hh` helpers over
     * entry->relpath itself (nor any path containing it), since it may actually be a datablock
     * name inside .blend file, which can have slashes and backslashes! See #46827.
     * Note that in the end, this means we 'cache' valid relative subdir once here,
     * this is actually better. */
    STRNCPY(rel_subdir, subdir);
    BLI_path_abs(rel_subdir, root);
    BLI_path_normalize_dir(rel_subdir, sizeof(rel_subdir));
    BLI_path_rel(rel_subdir, root);

    /* Update the current relative base path within the filelist root. */
    STRNCPY(job_params->cur_relbase, rel_subdir);

    bool is_lib = false;
    if (do_lib) {
      ListLibOptions list_lib_options = LIST_LIB_OPTION_NONE;
      if (!skip_currpar) {
        list_lib_options |= LIST_LIB_ADD_PARENT;
      }

      /* Libraries are loaded recursively when max_recursion is set. It doesn't check if there is
       * still a recursion level over. */
      if (max_recursion > 0) {
        list_lib_options |= LIST_LIB_RECURSIVE;
      }
      /* Only load assets when browsing an asset library. For normal file browsing we return all
       * entries. `FLF_ASSETS_ONLY` filter can be enabled/disabled by the user. */
      if (job_params->load_asset_library) {
        list_lib_options |= LIST_LIB_ASSETS_ONLY;
      }
      std::optional<int> lib_entries_num = filelist_readjob_list_lib(
          job_params, subdir, &entries, list_lib_options, &indexer_runtime);
      if (lib_entries_num) {
        is_lib = true;
        entries_num += *lib_entries_num;
      }
    }

    if (!is_lib && BLI_is_dir(subdir)) {
      entries_num = filelist_readjob_list_dir(job_params,
                                              subdir,
                                              &entries,
                                              filter_glob,
                                              do_lib,
                                              job_params->main_filepath,
                                              skip_currpar);
    }

    LISTBASE_FOREACH (FileListInternEntry *, entry, &entries) {
      entry->uid = filelist_uid_generate(filelist);
      entry->name = fileentry_uiname(root, entry, dir);
      entry->free_name = true;

      if (filelist_readjob_should_recurse_into_entry(
              max_recursion, is_lib, recursion_level, entry))
      {
        /* We have a directory we want to list, add it to todo list!
         * Using #BLI_path_join works but isn't needed as `root` has a trailing slash. */
        BLI_string_join(dir, sizeof(dir), root, entry->relpath);
        BLI_path_abs(dir, job_params->main_filepath);
        BLI_path_normalize_dir(dir, sizeof(dir));
        td_dir = static_cast<TodoDir *>(BLI_stack_push_r(todo_dirs));
        td_dir->level = recursion_level + 1;
        td_dir->dir = BLI_strdup(dir);
        dirs_todo_count++;
      }
    }

    if (filelist_readjob_append_entries(job_params, &entries, entries_num)) {
      *do_update = true;
    }

    dirs_done_count++;
    *progress = float(dirs_done_count) / float(dirs_todo_count);
    MEM_freeN(subdir);
  }

  /* Finalize and free indexer. */
  if (indexer_runtime.callbacks->filelist_finished && BLI_stack_is_empty(todo_dirs)) {
    indexer_runtime.callbacks->filelist_finished(indexer_runtime.user_data);
  }
  if (indexer_runtime.callbacks->free_user_data && indexer_runtime.user_data) {
    indexer_runtime.callbacks->free_user_data(indexer_runtime.user_data);
    indexer_runtime.user_data = nullptr;
  }

  /* If we were interrupted by stop, stack may not be empty and we need to free
   * pending dir paths. */
  while (!BLI_stack_is_empty(todo_dirs)) {
    td_dir = static_cast<TodoDir *>(BLI_stack_peek(todo_dirs));
    MEM_freeN(td_dir->dir);
    BLI_stack_discard(todo_dirs);
  }
  BLI_stack_free(todo_dirs);
}

static void filelist_readjob_do(const bool do_lib,
                                FileListReadJob *job_params,
                                const bool *stop,
                                bool *do_update,
                                float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  //  BLI_assert(filelist->filtered == nullptr);
  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  /* A valid, but empty directory from now. */
  filelist->filelist.entries_num = 0;

  filelist_readjob_recursive_dir_add_items(do_lib, job_params, stop, do_update, progress);
}

static void filelist_readjob_dir(FileListReadJob *job_params,
                                 bool *stop,
                                 bool *do_update,
                                 float *progress)
{
  filelist_readjob_do(false, job_params, stop, do_update, progress);
}

static void filelist_readjob_lib(FileListReadJob *job_params,
                                 bool *stop,
                                 bool *do_update,
                                 float *progress)
{
  filelist_readjob_do(true, job_params, stop, do_update, progress);
}

/**
 * Load asset library data, which currently means loading the asset catalogs for the library.
 */
static void filelist_readjob_load_asset_library_data(FileListReadJob *job_params, bool *do_update)
{
  FileList *tmp_filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  *do_update = false;

  if (job_params->filelist->asset_library_ref == nullptr) {
    return;
  }
  if (tmp_filelist->asset_library != nullptr && job_params->reload_asset_library == false) {
    /* Asset library itself is already loaded. Load assets into this. */
    job_params->load_asset_library = tmp_filelist->asset_library;
    return;
  }

  /* Load asset catalogs, into the temp filelist for thread-safety.
   * #filelist_readjob_endjob() will move it into the real filelist. */
  tmp_filelist->asset_library = AS_asset_library_load(job_params->current_main,
                                                      *job_params->filelist->asset_library_ref);
  /* Set asset library to load (may be overridden later for loading nested ones). */
  job_params->load_asset_library = tmp_filelist->asset_library;
  *do_update = true;
}

static void filelist_readjob_main_assets_add_items(FileListReadJob *job_params,
                                                   bool * /*stop*/,
                                                   bool *do_update,
                                                   float * /*progress*/)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  FileListInternEntry *entry;
  ListBase tmp_entries = {nullptr};
  ID *id_iter;
  int entries_num = 0;

  /* Make sure no IDs are added/removed/reallocated in the main thread while this is running in
   * parallel. */
  BKE_main_lock(job_params->current_main);

  FOREACH_MAIN_ID_BEGIN (job_params->current_main, id_iter) {
    if (!id_iter->asset_data || ID_IS_LINKED(id_iter)) {
      continue;
    }

    const char *id_code_name = BKE_idtype_idcode_to_name(GS(id_iter->name));

    entry = MEM_new<FileListInternEntry>(__func__);
    std::string datablock_path = StringRef(id_code_name) + SEP_STR + (id_iter->name + 2);
    entry->relpath = current_relpath_append(job_params, datablock_path.c_str());
    entry->name = id_iter->name + 2;
    entry->free_name = false;
    entry->typeflag |= FILE_TYPE_BLENDERLIB | FILE_TYPE_ASSET;
    entry->blentype = GS(id_iter->name);
    entry->uid = filelist_uid_generate(filelist);
    entry->local_data.preview_image = BKE_asset_metadata_preview_get_from_id(id_iter->asset_data,
                                                                             id_iter);
    entry->local_data.id = id_iter;
    if (job_params->load_asset_library) {
      entry->asset = job_params->load_asset_library->add_local_id_asset(*id_iter);
    }
    entries_num++;
    BLI_addtail(&tmp_entries, entry);
  }
  FOREACH_MAIN_ID_END;

  BKE_main_unlock(job_params->current_main);

  if (entries_num) {
    *do_update = true;

    BLI_movelisttolist(&filelist->filelist.entries, &tmp_entries);
    filelist->filelist.entries_num += entries_num;
    filelist->filelist.entries_filtered_num = -1;
  }
}

/**
 * Check if \a bmain is stored within the root path of \a filelist. This means either directly or
 * in some nested directory. In other words, it checks if the \a filelist root path is contained in
 * the path to \a bmain.
 * This is irrespective of the recursion level displayed, it basically assumes unlimited recursion
 * levels.
 */
static bool filelist_contains_main(const FileList *filelist, const Main *bmain)
{
  if (filelist->asset_library_ref && (filelist->asset_library_ref->type == ASSET_LIBRARY_ALL)) {
    return true;
  }

  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  return blendfile_path[0] && BLI_path_contains(filelist->filelist.root, blendfile_path);
}

static void filelist_readjob_asset_library(FileListReadJob *job_params,
                                           bool *stop,
                                           bool *do_update,
                                           float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */

  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  /* A valid, but empty file-list from now. */
  filelist->filelist.entries_num = 0;

  BLI_assert(job_params->filelist->asset_library_ref != nullptr);

  /* NOP if already read. */
  filelist_readjob_load_asset_library_data(job_params, do_update);

  if (filelist_contains_main(filelist, job_params->current_main)) {
    asset_system::AssetLibrary *original_file_library = job_params->load_asset_library;
    job_params->load_asset_library = AS_asset_library_load(
        job_params->current_main, asset_system::current_file_library_reference());
    filelist_readjob_main_assets_add_items(job_params, stop, do_update, progress);
    job_params->load_asset_library = original_file_library;
  }
  if (!job_params->only_main_data) {
    filelist_readjob_recursive_dir_add_items(true, job_params, stop, do_update, progress);
  }
}

static void filelist_readjob_main(FileListReadJob *job_params,
                                  bool *stop,
                                  bool *do_update,
                                  float *progress)
{
  /* TODO! */
  filelist_readjob_dir(job_params, stop, do_update, progress);
}

static void filelist_readjob_main_assets(FileListReadJob *job_params,
                                         bool *stop,
                                         bool *do_update,
                                         float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  filelist_readjob_load_asset_library_data(job_params, do_update);

  /* A valid, but empty file-list from now. */
  filelist->filelist.entries_num = 0;

  filelist_readjob_main_assets_add_items(job_params, stop, do_update, progress);
}

static void filelist_readjob_all_asset_library(FileListReadJob *job_params,
                                               bool *stop,
                                               bool *do_update,
                                               float *progress)
{
  FileList *filelist = job_params->tmp_filelist; /* Use the thread-safe filelist queue. */
  BLI_assert(BLI_listbase_is_empty(&filelist->filelist.entries) &&
             (filelist->filelist.entries_num == FILEDIR_NBR_ENTRIES_UNSET));

  filelist_readjob_load_asset_library_data(job_params, do_update);

  /* A valid, but empty file-list from now. */
  filelist->filelist.entries_num = 0;

  asset_system::AssetLibrary *current_file_library;
  {
    AssetLibraryReference library_ref{};
    library_ref.custom_library_index = -1;
    library_ref.type = ASSET_LIBRARY_LOCAL;

    current_file_library = AS_asset_library_load(job_params->current_main, library_ref);
  }

  job_params->load_asset_library = current_file_library;
  filelist_readjob_main_assets_add_items(job_params, stop, do_update, progress);

  /* When only doing partially reload for main data, we're done. */
  if (job_params->only_main_data) {
    return;
  }

  /* Count how many asset libraries need to be loaded, for progress reporting. Not very precise. */
  int library_count = 0;
  asset_system::AssetLibrary::foreach_loaded([&library_count](auto &) { library_count++; }, false);

  BLI_assert(filelist->asset_library != nullptr);

  int libraries_done_count = 0;
  /* The "All" asset library was loaded, which means all other asset libraries are also loaded.
   * Load their assets from disk into the "All" library. */
  asset_system::AssetLibrary::foreach_loaded(
      [&](asset_system::AssetLibrary &nested_library) {
        StringRefNull root_path = nested_library.root_path();
        if (root_path.is_empty()) {
          return;
        }
        if (&nested_library == current_file_library) {
          /* Skip the "Current File" library, it's already loaded above. */
          return;
        }

        /* Override library info to read this library. */
        job_params->load_asset_library = &nested_library;
        STRNCPY(filelist->filelist.root, root_path.c_str());

        float progress_this = 0.0f;
        filelist_readjob_recursive_dir_add_items(
            true, job_params, stop, do_update, &progress_this);

        libraries_done_count++;
        *progress = float(libraries_done_count) / library_count;
      },
      false);
}

/**
 * Check if the read-job is requesting a partial reread of the file list only.
 */
static bool filelist_readjob_is_partial_read(const FileListReadJob *read_job)
{
  return read_job->only_main_data;
}

/**
 * \note This may trigger partial filelist reading. If the #FL_FORCE_RESET_MAIN_FILES flag is set,
 *       some current entries are kept and we just call the readjob to update the main files (see
 *       #FileListReadJob.only_main_data).
 */
static void filelist_readjob_startjob(void *flrjv, wmJobWorkerStatus *worker_status)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);

  //  printf("START filelist reading (%d files, main thread: %d)\n",
  //         flrj->filelist->filelist.entries_num, BLI_thread_is_main());

  {
    std::scoped_lock lock(flrj->lock);
    BLI_assert((flrj->tmp_filelist == nullptr) && flrj->filelist);

    flrj->tmp_filelist = static_cast<FileList *>(MEM_dupallocN(flrj->filelist));

    BLI_listbase_clear(&flrj->tmp_filelist->filelist.entries);
    flrj->tmp_filelist->filelist.entries_num = FILEDIR_NBR_ENTRIES_UNSET;

    flrj->tmp_filelist->filelist_intern.filtered = nullptr;
    BLI_listbase_clear(&flrj->tmp_filelist->filelist_intern.entries);
    if (filelist_readjob_is_partial_read(flrj)) {
      /* Don't unset the current UID on partial read, would give duplicates otherwise. */
    }
    else {
      filelist_uid_unset(&flrj->tmp_filelist->filelist_intern.curr_uid);
    }

    flrj->tmp_filelist->libfiledata = nullptr;
    flrj->tmp_filelist->filelist_cache = nullptr;
    flrj->tmp_filelist->selection_state = nullptr;
    flrj->tmp_filelist->asset_library_ref = nullptr;
    flrj->tmp_filelist->filter_data.asset_catalog_filter = nullptr;
  }

  flrj->tmp_filelist->read_job_fn(
      flrj, &worker_status->stop, &worker_status->do_update, &worker_status->progress);
}

/**
 * \note This may update for a partial filelist reading job. If the #FL_FORCE_RESET_MAIN_FILES flag
 *       is set, some current entries are kept and we just call the readjob to update the main
 *       files (see #FileListReadJob.only_main_data).
 */
static void filelist_readjob_update(void *flrjv)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);
  FileListIntern *fl_intern = &flrj->filelist->filelist_intern;
  ListBase new_entries = {nullptr};
  int entries_num, new_entries_num = 0;

  BLI_movelisttolist(&new_entries, &fl_intern->entries);
  entries_num = flrj->filelist->filelist.entries_num;

  {
    std::scoped_lock lock(flrj->lock);
    if (flrj->tmp_filelist->filelist.entries_num > 0) {
      /* We just move everything out of 'thread context' into final list. */
      new_entries_num = flrj->tmp_filelist->filelist.entries_num;
      BLI_movelisttolist(&new_entries, &flrj->tmp_filelist->filelist.entries);
      flrj->tmp_filelist->filelist.entries_num = 0;
    }

    if (flrj->tmp_filelist->asset_library) {
      flrj->filelist->asset_library = flrj->tmp_filelist->asset_library;
    }

    /* Important for partial reads: Copy increased UID counter back to the real list. */
    fl_intern->curr_uid = std::max(flrj->tmp_filelist->filelist_intern.curr_uid,
                                   fl_intern->curr_uid);
  }

  if (new_entries_num) {
    /* Do not clear selection cache, we can assume already 'selected' UIDs are still valid! Keep
     * the asset library data we just read. */
    filelist_clear_ex(flrj->filelist, false, true, false);

    flrj->filelist->flags |= (FL_NEED_SORTING | FL_NEED_FILTERING);
  }

  /* if no new_entries_num, this is NOP */
  BLI_movelisttolist(&fl_intern->entries, &new_entries);
  flrj->filelist->filelist.entries_num = std::max(entries_num, 0) + new_entries_num;
}

static void filelist_readjob_endjob(void *flrjv)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);

  /* In case there would be some dangling update... */
  filelist_readjob_update(flrjv);

  flrj->filelist->flags &= ~FL_IS_PENDING;
  flrj->filelist->flags |= FL_IS_READY;
}

static void filelist_readjob_free(void *flrjv)
{
  FileListReadJob *flrj = static_cast<FileListReadJob *>(flrjv);

  //  printf("END filelist reading (%d files)\n", flrj->filelist->filelist.entries_num);

  if (flrj->tmp_filelist) {
    /* tmp_filelist shall never ever be filtered! */
    BLI_assert(flrj->tmp_filelist->filelist.entries_num == 0);
    BLI_assert(BLI_listbase_is_empty(&flrj->tmp_filelist->filelist.entries));

    filelist_freelib(flrj->tmp_filelist);
    filelist_free(flrj->tmp_filelist);
  }

  MEM_delete(flrj);
}

static eWM_JobType filelist_jobtype_get(const FileList *filelist)
{
  if (filelist->asset_library_ref) {
    return WM_JOB_TYPE_ASSET_LIBRARY_LOAD;
  }
  return WM_JOB_TYPE_FILESEL_READDIR;
}

/* TODO(Julian): This is temporary, because currently the job system identifies jobs to suspend by
 * the startjob callback, rather than the type. See PR #123033. */
static void assetlibrary_readjob_startjob(void *flrjv, wmJobWorkerStatus *worker_status)
{
  filelist_readjob_startjob(flrjv, worker_status);
}

static void filelist_readjob_start_ex(FileList *filelist,
                                      const int space_notifier,
                                      const bContext *C,
                                      const bool force_blocking_read)
{
  Main *bmain = CTX_data_main(C);
  wmJob *wm_job;
  FileListReadJob *flrj;

  if (!filelist_is_dir(filelist, filelist->filelist.root)) {
    return;
  }

  /* prepare job data */
  flrj = MEM_new<FileListReadJob>(__func__);
  flrj->filelist = filelist;
  flrj->current_main = bmain;
  STRNCPY(flrj->main_filepath, BKE_main_blendfile_path(bmain));
  if ((filelist->flags & FL_FORCE_RESET_MAIN_FILES) && !(filelist->flags & FL_FORCE_RESET) &&
      (filelist->filelist.entries_num != FILEDIR_NBR_ENTRIES_UNSET))
  {
    flrj->only_main_data = true;
  }
  if (filelist->flags & FL_RELOAD_ASSET_LIBRARY) {
    flrj->reload_asset_library = true;
  }

  filelist->flags &= ~(FL_FORCE_RESET | FL_FORCE_RESET_MAIN_FILES | FL_RELOAD_ASSET_LIBRARY |
                       FL_IS_READY);
  filelist->flags |= FL_IS_PENDING;

  /* The file list type may not support threading so execute immediately. Same when only rereading
   * #Main data (which we do quite often on changes to #Main, since it's the easiest and safest way
   * to ensure the displayed data is up to date), because some operations executing right after
   * main data changed may need access to the ID files (see #93691). */
  const bool no_threads = (filelist->tags & FILELIST_TAGS_NO_THREADS) || flrj->only_main_data;

  if (force_blocking_read || no_threads) {
    /* Single threaded execution. Just directly call the callbacks. */
    wmJobWorkerStatus worker_status = {};
    filelist_readjob_startjob(flrj, &worker_status);
    filelist_readjob_endjob(flrj);
    filelist_readjob_free(flrj);

    WM_event_add_notifier(C, space_notifier | NA_JOB_FINISHED, nullptr);
    return;
  }

  /* setup job */
  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       filelist,
                       "Listing directories...",
                       WM_JOB_PROGRESS,
                       filelist_jobtype_get(filelist));
  WM_jobs_customdata_set(wm_job, flrj, filelist_readjob_free);
  WM_jobs_timer(wm_job, 0.01, space_notifier, space_notifier | NA_JOB_FINISHED);
  WM_jobs_callbacks(wm_job,
                    filelist->asset_library_ref ? assetlibrary_readjob_startjob :
                                                  filelist_readjob_startjob,
                    nullptr,
                    filelist_readjob_update,
                    filelist_readjob_endjob);

  /* start the job */
  WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void filelist_readjob_start(FileList *filelist, const int space_notifier, const bContext *C)
{
  filelist_readjob_start_ex(filelist, space_notifier, C, false);
}

void filelist_readjob_blocking_run(FileList *filelist, int space_notifier, const bContext *C)
{
  filelist_readjob_start_ex(filelist, space_notifier, C, true);
}

void filelist_readjob_stop(FileList *filelist, wmWindowManager *wm)
{
  WM_jobs_kill_type(wm, filelist, filelist_jobtype_get(filelist));
}

int filelist_readjob_running(FileList *filelist, wmWindowManager *wm)
{
  return WM_jobs_test(wm, filelist, filelist_jobtype_get(filelist));
}
