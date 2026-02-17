/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

/* global includes */

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
#include <sys/stat.h>

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"
#include "AS_remote_library.hh"

#include "MEM_guardedalloc.h"

#include "BLF_api.hh"

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "BKE_asset.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_main.hh"
#include "BKE_preferences.h"
#include "BKE_preview_image.hh"

#include "DNA_asset_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "ED_asset_indexer.hh"
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
#include "filelist_readjob.hh"

namespace blender {

static ImBuf *gSpecialFileImages[int(SpecialFileImages::_Max)];

static void remote_asset_library_refresh_online_assets_status(const FileList *filelist)
{
  for (FileListInternEntry &entry : filelist->filelist_intern.entries) {
    if ((entry.typeflag & FILE_TYPE_ASSET_ONLINE) == 0) {
      continue;
    }

    /* #AssetRepresentation.full_library_path() will only return a non-empty string if the asset's
     * path points into some .blend on disk. */
    std::shared_ptr<asset_system::AssetRepresentation> asset = entry.asset.lock();
    std::string filepath = asset->full_library_path();
    if (filepath.empty()) {
      continue;
    }
    BLI_assert(BLI_is_file(filepath.c_str()));

    entry.typeflag &= ~FILE_TYPE_ASSET_ONLINE;
    asset->online_asset_mark_downloaded();

    if (FileDirEntry **cached_entry = filelist->filelist_cache->uids.lookup_ptr(entry.uid)) {
      (**cached_entry).typeflag &= ~FILE_TYPE_ASSET_ONLINE;
    }
  }
}

void filelist_remote_asset_library_refresh_online_assets_status(
    const FileList *filelist, const blender::StringRef remote_url)
{
  if (!filelist->asset_library || !filelist->asset_library_ref) {
    return;
  }
  if (remote_url.is_empty()) {
    return;
  }

  if ((filelist->asset_library_ref->type == ASSET_LIBRARY_ALL) ||
      (filelist->asset_library->remote_url() == remote_url))
  {
    remote_asset_library_refresh_online_assets_status(filelist);
  }
}

void filelist_setindexer(FileList *filelist, const FileIndexerType *indexer)
{
  BLI_assert(filelist);
  BLI_assert(indexer);

  filelist->indexer = indexer;
}

void filelist_set_asset_include_online(FileList *filelist, const bool show_online_assets)
{
  if (!filelist->asset_library_ref ||
      !asset_system::is_or_contains_remote_libraries(*filelist->asset_library_ref))
  {
    /* Online flag has no effect if not displaying online libraries. This function will be called
     * again when switching libraries, so updating the flag here shouldn't be needed. Still do it
     * for predictability. */
    SET_FLAG_FROM_TEST(filelist->flags, show_online_assets, FL_ASSETS_INCLUDE_ONLINE);
    return;
  }

  if (show_online_assets && ((filelist->flags & FL_ASSETS_INCLUDE_ONLINE) == 0)) {
    /* Full refresh when enabling online assets, so online asset loading is triggered. */
    filelist_tag_force_reset(filelist);
    filelist->flags |= FL_ASSETS_INCLUDE_ONLINE;
  }
  else if (!show_online_assets && ((filelist->flags & FL_ASSETS_INCLUDE_ONLINE) != 0)) {
    /* Simply filter out online assets when they were already loaded. */
    filelist_tag_needs_filtering(filelist);
    filelist->flags &= ~FL_ASSETS_INCLUDE_ONLINE;
  }
}

void filelist_set_asset_catalog_filter_options(
    FileList *filelist,
    eFileSel_Params_AssetCatalogVisibility catalog_visibility,
    const bUUID *catalog_id)
{
  if (!filelist->filter_data.asset_catalog_filter) {
    /* There's no filter data yet. */
    filelist->filter_data.asset_catalog_filter =
        ed::asset_browser::file_create_asset_catalog_filter_settings();
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
      MEM_SAFE_DELETE(filelist->asset_library_ref);
      filelist->flags |= FL_FORCE_RESET;
    }
    return;
  }

  if (!filelist->asset_library_ref) {
    filelist->asset_library_ref = MEM_new<AssetLibraryReference>("filelist asset library");
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
  if (!filelist_ready) {
    return true;
  }
  const PreviewImage *asset_preview = file->asset ? file->asset->get_preview() : nullptr;
  if (asset_preview && (asset_preview->flag[ICON_SIZE_PREVIEW] & PRV_RENDERING)) {
    return true;
  }

  return file->flags & FILE_ENTRY_PREVIEW_LOADING;
}

static FileDirEntry *filelist_geticon_get_file(FileList *filelist, const int index)
{
  BLI_assert(G.background == false);

  return filelist_file(filelist, index);
}

ImBuf *filelist_file_get_preview_image(const FileDirEntry *file)
{
  return (file->preview_icon_id && BKE_icon_is_imbuf(file->preview_icon_id)) ?
             BKE_icon_imbuf_get_buffer(file->preview_icon_id) :
             nullptr;
}

static ImBuf *filelist_ensure_special_file_image(SpecialFileImages image, int icon)
{
  ImBuf *ibuf = gSpecialFileImages[int(image)];
  if (ibuf) {
    return ibuf;
  }
  return gSpecialFileImages[int(image)] = ui::svg_icon_bitmap(icon, 256.0f, false);
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
  const eFileSel_File_Types typeflag = eFileSel_File_Types(file->typeflag);

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
    const int ret = ui::icon_from_idcode(file->blentype);
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

static void filelist_entry_clear(FileDirEntry *entry)
{
  if (entry->name && ((entry->flags & FILE_ENTRY_NAME_FREE) != 0)) {
    MEM_delete(const_cast<char *>(entry->name));
  }
  if (entry->relpath) {
    MEM_delete(entry->relpath);
  }
  if (entry->redirection_path) {
    MEM_delete(entry->redirection_path);
  }
  if (entry->preview_icon_id &&
      /* Online assets previews are managed by the general UI preview system, not the file browser
       * one. Don't mess with them. */
      ((entry->typeflag & FILE_TYPE_ASSET_ONLINE) == 0))
  {
    BKE_icon_delete(entry->preview_icon_id);
    entry->preview_icon_id = 0;
  }
}

static void filelist_entry_free(FileDirEntry *entry)
{
  filelist_entry_clear(entry);
  MEM_delete(entry);
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
    MEM_delete(entry->relpath);
  }
  if (entry->redirection_path) {
    MEM_delete(entry->redirection_path);
  }
  if (entry->name && entry->free_name) {
    MEM_delete(const_cast<char *>(entry->name));
  }
  MEM_delete(entry);
}

static void filelist_intern_free(FileList *filelist)
{
  FileListIntern *filelist_intern = &filelist->filelist_intern;
  for (FileListInternEntry &entry : filelist_intern->entries.items_mutable()) {
    filelist_intern_entry_free(filelist, &entry);
  }
  BLI_listbase_clear(&filelist_intern->entries);

  MEM_SAFE_DELETE(filelist_intern->filtered);
}

/**
 * \return the number of main files removed.
 */
static int filelist_intern_free_main_files(FileList *filelist)
{
  FileListIntern *filelist_intern = &filelist->filelist_intern;
  int removed_counter = 0;
  for (FileListInternEntry &entry : filelist_intern->entries.items_mutable()) {
    if (!filelist_intern_entry_is_main_file(&entry)) {
      continue;
    }

    BLI_remlink(&filelist_intern->entries, &entry);
    filelist_intern_entry_free(filelist, &entry);
    removed_counter++;
  }

  if (removed_counter > 0) {
    MEM_SAFE_DELETE(filelist_intern->filtered);
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
  BLI_assert((preview->flags & FILE_TYPE_ASSET_ONLINE) == 0);

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
    MEM_delete(preview_taskdata->preview);
  }

  MEM_delete(preview_taskdata);
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

    for (FileDirEntry &entry : cache->cached_entries) {
      entry.flags &= ~FILE_ENTRY_PREVIEW_LOADING;
    }

    FileListEntryPreview *preview;
    while ((preview = static_cast<FileListEntryPreview *>(
                BLI_thread_queue_pop_timeout(cache->previews_done, 0))))
    {
      // printf("%s: DONE %d - %s - %p\n", __func__, preview->index, preview->path,
      // preview->img);
      BLI_assert((preview->flags & FILE_TYPE_ASSET_ONLINE) == 0);
      if (preview->icon_id) {
        BKE_icon_delete(preview->icon_id);
      }
      MEM_delete(preview);
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

void filelist_online_asset_preview_request(const bContext *C, FileDirEntry *entry)
{
  BLI_assert(entry->asset);
  BLI_assert(entry->asset->is_online());

  if (entry->preview_icon_id) {
    return;
  }

  if (!filelist_file_preview_load_poll(entry)) {
    return;
  }

  /* Request online preview if needed. */
  if (entry->asset->is_online()) {
    entry->asset->ensure_previewable(*C, CTX_wm_reports(C));
    entry->preview_icon_id = entry->asset->get_preview()->runtime->icon_id;
  }
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

  if (entry->typeflag & FILE_TYPE_ASSET_ONLINE) {
    /* Online assets use the UI system for async preview loading (see #PreviewLoadJob)
     * instead of the file browser one. */
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

  FileListEntryPreview *preview = MEM_new_zeroed<FileListEntryPreview>(__func__);
  preview->index = index;
  preview->flags = entry->typeflag;
  preview->icon_id = 0;

  if (preview_in_memory) {
    /* TODO(mano-wii): No need to use the thread API here. */
    BLI_assert(!BKE_previewimg_is_rendering(preview_in_memory, ICON_SIZE_PREVIEW));
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

    FileListEntryPreviewTaskData *preview_taskdata = MEM_new_zeroed<FileListEntryPreviewTaskData>(
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
      MEM_new_uninitialized(sizeof(*this->block_entries) * this->size, __func__));

  this->misc_entries.reserve(this->size);
  this->misc_entries_indices = MEM_new_array_uninitialized<int>(this->size, __func__);
  copy_vn_i(this->misc_entries_indices, this->size, -1);

  this->uids.reserve(this->size * 2);
}

FileListEntryCache::~FileListEntryCache()
{
  filelist_cache_previews_free(this);

  MEM_delete(this->block_entries);
  MEM_delete(this->misc_entries_indices);

  for (FileDirEntry &entry : this->cached_entries.items_mutable()) {
    filelist_entry_free(&entry);
  }
}

void filelist_cache_clear(FileListEntryCache *cache, size_t new_size)
{
  filelist_cache_previews_clear(cache);

  cache->block_cursor = cache->block_start_index = cache->block_center_index =
      cache->block_end_index = 0;
  if (new_size != cache->size) {
    cache->block_entries = static_cast<FileDirEntry **>(
        MEM_realloc_uninitialized(cache->block_entries, sizeof(*cache->block_entries) * new_size));
  }

  cache->misc_entries.clear();
  cache->misc_entries.reserve(new_size);
  if (new_size != cache->size) {
    cache->misc_entries_indices = static_cast<int *>(MEM_realloc_uninitialized(
        cache->misc_entries_indices, sizeof(*cache->misc_entries_indices) * new_size));
  }
  copy_vn_i(cache->misc_entries_indices, new_size, -1);

  cache->uids.clear();
  cache->uids.reserve(new_size * 2);

  cache->size = new_size;

  for (FileDirEntry &entry : cache->cached_entries.items_mutable()) {
    filelist_entry_free(&entry);
  }
  BLI_listbase_clear(&cache->cached_entries);
}

FileList *filelist_new(short type)
{
  FileList *p = MEM_new<FileList>(__func__);

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

  filelist->type = eFileSelectType(type);
  filelist->tags = 0;
  filelist->indexer = &file_indexer_noop;
  filelist->check_dir_fn = nullptr;
  filelist->start_job_fn = nullptr;
  filelist->timer_step_fn = nullptr;
  filelist->read_job_fn = nullptr;
  filelist->prepare_filter_fn = nullptr;
  filelist->filter_fn = nullptr;

  switch (filelist->type) {
    case FILE_MAIN:
      filelist_set_readjob_main(filelist);
      break;
    case FILE_LOADLIB:
      filelist_set_readjob_library(filelist);
      break;
    case FILE_ASSET_LIBRARY:
      filelist_set_readjob_on_disk_asset_library(filelist);
      break;
    case FILE_MAIN_ASSET:
      filelist_set_readjob_current_file_asset_library(filelist);
      break;
    case FILE_ASSET_LIBRARY_REMOTE:
      filelist_set_readjob_remote_asset_library(filelist);
      break;
    case FILE_ASSET_LIBRARY_ALL:
      filelist_set_readjob_all_asset_library(filelist);
      break;
    default:
      filelist_set_readjob_directories(filelist);
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

  MEM_SAFE_DELETE(filelist->asset_library_ref);

  memset(&filelist->filter_data, 0, sizeof(filelist->filter_data));

  filelist->flags &= ~(FL_NEED_SORTING | FL_NEED_FILTERING);

  MEM_delete(filelist);
}

asset_system::AssetLibrary *filelist_asset_library(FileList *filelist)
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

const char *fileentry_uiname(const char *root, FileListInternEntry *entry, char *buff)
{
  if (asset_system::AssetRepresentation *asset = entry->get_asset()) {
    const StringRefNull asset_name = asset->get_name();
    return BLI_strdupn(asset_name.c_str(), asset_name.size());
  }

  const char *relpath = entry->relpath;
  const eFileSel_File_Types typeflag = entry->typeflag;
  char *name = nullptr;

  if (typeflag & FILE_TYPE_FTFONT && !(typeflag & FILE_TYPE_BLENDERLIB)) {
    if (entry->redirection_path) {
      name = BLF_display_name_from_file(entry->redirection_path);
    }
    else {
      char abspath[FILE_MAX_LIBEXTRA];
      BLI_path_join(abspath, sizeof(abspath), root, relpath);
      name = BLF_display_name_from_file(abspath);
    }
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
      name = const_cast<char *>(relpath);
    }
    else {
      name = const_cast<char *>(BLI_path_basename(relpath));
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
  return filelist->check_dir_fn(filelist, const_cast<char *>(path), false);
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

  ret = MEM_new<FileDirEntry>(__func__);

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

  return cache->misc_entries.lookup_default(index, nullptr);
}

FileDirEntry *filelist_file_ex(FileList *filelist, const int index, const bool use_request)
{
  FileDirEntry *ret = nullptr;
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
  if (FileDirEntry *old = cache->misc_entries.pop_default(old_index, nullptr)) {
    cache->uids.remove(old->uid);
    filelist_file_release_entry(filelist, old);
  }
  cache->misc_entries.add(index, ret);
  cache->uids.add(ret->uid, ret);

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

FileUID filelist_uid_generate(FileList *filelist)
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
    if ((entry = cache->misc_entries.pop_default(idx, nullptr)) == nullptr) {
      entry = filelist_file_create_entry(filelist, idx);
      cache->uids.add(entry->uid, entry);
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
    cache->uids.remove(entry->uid);
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
      BLI_assert_unreachable();
      continue;
    }
    /* entry might have been removed from cache in the mean time,
     * we do not want to cache it again here. */
    entry = filelist_file_ex(filelist, preview->index, false);

    BLI_assert_msg((entry->typeflag & FILE_TYPE_ASSET_ONLINE) == 0,
                   "Online assets shouldn't use the file preview loading system");

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

    MEM_delete(preview);
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
                                 ".toml",
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

}  // namespace blender
