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
 */

/** \file
 * \ingroup edasset
 *
 * Abstractions to manage runtime asset lists with a global cache for multiple UI elements to
 * access.
 * Internally this uses the #FileList API and structures from `filelist.c`. This is just because it
 * contains most necessary logic already and there's not much time for a more long-term solution.
 */

#include <optional>
#include <string>

#include "BKE_context.h"

#include "BLI_map.hh"
#include "BLI_path_util.h"
#include "BLI_utility_mixins.hh"

#include "DNA_asset_types.h"
#include "DNA_space_types.h"

#include "BKE_preferences.h"

#include "ED_fileselect.h"

#include "WM_api.h"
#include "WM_types.h"

/* XXX uses private header of file-space. */
#include "../space_file/filelist.h"

#include "ED_asset_handle.h"
#include "ED_asset_list.h"
#include "ED_asset_list.hh"
#include "asset_library_reference.hh"

namespace blender::ed::asset {

/* -------------------------------------------------------------------- */
/** \name Asset list API
 *
 *  Internally re-uses #FileList from the File Browser. It does all the heavy lifting already.
 * \{ */

/**
 * RAII wrapper for `FileList`
 */
class FileListWrapper {
  static void filelist_free_fn(FileList *list)
  {
    filelist_free(list);
    MEM_freeN(list);
  }

  std::unique_ptr<FileList, decltype(&filelist_free_fn)> file_list_;

 public:
  explicit FileListWrapper(eFileSelectType filesel_type)
      : file_list_(filelist_new(filesel_type), filelist_free_fn)
  {
  }
  FileListWrapper(FileListWrapper &&other) = default;
  FileListWrapper &operator=(FileListWrapper &&other) = default;
  ~FileListWrapper()
  {
    /* Destructs the owned pointer. */
    file_list_ = nullptr;
  }

  operator FileList *() const
  {
    return file_list_.get();
  }
};

class PreviewTimer {
  /* Non-owning! The Window-Manager registers and owns this. */
  wmTimer *timer_ = nullptr;

 public:
  void ensureRunning(const bContext *C)
  {
    if (!timer_) {
      timer_ = WM_event_add_timer_notifier(
          CTX_wm_manager(C), CTX_wm_window(C), NC_ASSET | ND_ASSET_LIST_PREVIEW, 0.01);
    }
  }

  void stop(const bContext *C)
  {
    if (timer_) {
      WM_event_remove_timer_notifier(CTX_wm_manager(C), CTX_wm_window(C), timer_);
      timer_ = nullptr;
    }
  }
};

class AssetList : NonCopyable {
  FileListWrapper filelist_;
  AssetLibraryReference library_ref_;
  PreviewTimer previews_timer_;

 public:
  AssetList() = delete;
  AssetList(eFileSelectType filesel_type, const AssetLibraryReference &asset_library_ref);
  AssetList(AssetList &&other) = default;
  ~AssetList() = default;

  void setup();
  void fetch(const bContext &C);
  void ensurePreviewsJob(bContext *C);
  void clear(bContext *C);

  bool needsRefetch() const;
  void iterate(AssetListIterFn fn) const;
  bool listen(const wmNotifier &notifier) const;
  int size() const;
  void tagMainDataDirty() const;
  void remapID(ID *id_old, ID *id_new) const;
  StringRef filepath() const;
};

AssetList::AssetList(eFileSelectType filesel_type, const AssetLibraryReference &asset_library_ref)
    : filelist_(filesel_type), library_ref_(asset_library_ref)
{
}

void AssetList::setup()
{
  FileList *files = filelist_;

  bUserAssetLibrary *user_library = nullptr;

  /* Ensure valid repository, or fall-back to local one. */
  if (library_ref_.type == ASSET_LIBRARY_CUSTOM) {
    BLI_assert(library_ref_.custom_library_index >= 0);

    user_library = BKE_preferences_asset_library_find_from_index(
        &U, library_ref_.custom_library_index);
  }

  /* Relevant bits from file_refresh(). */
  /* TODO pass options properly. */
  filelist_setrecursion(files, FILE_SELECT_MAX_RECURSIONS);
  filelist_setsorting(files, FILE_SORT_ALPHA, false);
  filelist_setlibrary(files, &library_ref_);
  filelist_setfilter_options(
      files,
      false,
      true,
      true, /* Just always hide parent, prefer to not add an extra user option for this. */
      FILE_TYPE_BLENDERLIB,
      FILTER_ID_ALL,
      true,
      "",
      "");

  char path[FILE_MAXDIR] = "";
  if (user_library) {
    BLI_strncpy(path, user_library->path, sizeof(path));
    filelist_setdir(files, path);
  }
  else {
    filelist_setdir(files, path);
  }
}

void AssetList::fetch(const bContext &C)
{
  FileList *files = filelist_;

  if (filelist_needs_force_reset(files)) {
    filelist_readjob_stop(files, CTX_wm_manager(&C));
    filelist_clear_from_reset_tag(files);
  }

  if (filelist_needs_reading(files)) {
    if (!filelist_pending(files)) {
      filelist_readjob_start(files, NC_ASSET | ND_ASSET_LIST_READING, &C);
    }
  }
  filelist_sort(files);
  filelist_filter(files);
}

bool AssetList::needsRefetch() const
{
  return filelist_needs_force_reset(filelist_) || filelist_needs_reading(filelist_);
}

void AssetList::iterate(AssetListIterFn fn) const
{
  FileList *files = filelist_;
  int numfiles = filelist_files_ensure(files);

  for (int i = 0; i < numfiles; i++) {
    FileDirEntry *file = filelist_file(files, i);
    if ((file->typeflag & FILE_TYPE_ASSET) == 0) {
      continue;
    }

    AssetHandle asset_handle = {file};
    if (!fn(asset_handle)) {
      /* If the callback returns false, we stop iterating. */
      break;
    }
  }
}

void AssetList::ensurePreviewsJob(bContext *C)
{
  FileList *files = filelist_;
  int numfiles = filelist_files_ensure(files);

  filelist_cache_previews_set(files, true);
  filelist_file_cache_slidingwindow_set(files, 256);
  /* TODO fetch all previews for now. */
  filelist_file_cache_block(files, numfiles / 2);
  filelist_cache_previews_update(files);

  {
    const bool previews_running = filelist_cache_previews_running(files) &&
                                  !filelist_cache_previews_done(files);
    if (previews_running) {
      previews_timer_.ensureRunning(C);
    }
    else {
      /* Preview is not running, no need to keep generating update events! */
      previews_timer_.stop(C);
    }
  }
}

void AssetList::clear(bContext *C)
{
  /* Based on #ED_fileselect_clear() */

  FileList *files = filelist_;
  filelist_readjob_stop(files, CTX_wm_manager(C));
  filelist_freelib(files);
  filelist_clear(files);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST, nullptr);
}

/**
 * \return True if the asset-list needs a UI redraw.
 */
bool AssetList::listen(const wmNotifier &notifier) const
{
  switch (notifier.category) {
    case NC_ID: {
      if (ELEM(notifier.action, NA_RENAME)) {
        return true;
      }
      break;
    }
    case NC_ASSET:
      if (ELEM(notifier.data, ND_ASSET_LIST, ND_ASSET_LIST_READING, ND_ASSET_LIST_PREVIEW)) {
        return true;
      }
      if (ELEM(notifier.action, NA_ADDED, NA_REMOVED, NA_EDITED)) {
        return true;
      }
      break;
  }

  return false;
}

/**
 * \return The number of assets in the list.
 */
int AssetList::size() const
{
  return filelist_files_ensure(filelist_);
}

void AssetList::tagMainDataDirty() const
{
  if (filelist_needs_reset_on_main_changes(filelist_)) {
    filelist_tag_force_reset_mainfiles(filelist_);
  }
}

void AssetList::remapID(ID * /*id_old*/, ID * /*id_new*/) const
{
  /* Trigger full re-fetch of the file list if main data was changed, don't even attempt remap
   * pointers. We could give file list types a id-remap callback, but it's probably not worth it.
   * Refreshing local file lists is relatively cheap. */
  tagMainDataDirty();
}

StringRef AssetList::filepath() const
{
  return filelist_dir(filelist_);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Runtime asset list cache
 * \{ */

/**
 * Class managing a global asset list map, each entry being a list for a specific asset library.
 */
class AssetListStorage {
  using AssetListMap = Map<AssetLibraryReferenceWrapper, AssetList>;

 public:
  /* Purely static class, can't instantiate this. */
  AssetListStorage() = delete;

  static void fetch_library(const AssetLibraryReference &library_reference, const bContext &C);
  static void destruct();
  static AssetList *lookup_list(const AssetLibraryReference &library_ref);
  static void tagMainDataDirty();
  static void remapID(ID *id_new, ID *id_old);

 private:
  static std::optional<eFileSelectType> asset_library_reference_to_fileselect_type(
      const AssetLibraryReference &library_reference);

  using is_new_t = bool;
  static std::tuple<AssetList &, is_new_t> ensure_list_storage(
      const AssetLibraryReference &library_reference, eFileSelectType filesel_type);

  static AssetListMap &global_storage();
};

void AssetListStorage::fetch_library(const AssetLibraryReference &library_reference,
                                     const bContext &C)
{
  std::optional filesel_type = asset_library_reference_to_fileselect_type(library_reference);
  if (!filesel_type) {
    return;
  }

  auto [list, is_new] = ensure_list_storage(library_reference, *filesel_type);
  if (is_new || list.needsRefetch()) {
    list.setup();
    list.fetch(C);
  }
}

void AssetListStorage::destruct()
{
  global_storage().~AssetListMap();
}

AssetList *AssetListStorage::lookup_list(const AssetLibraryReference &library_ref)
{
  return global_storage().lookup_ptr(library_ref);
}

void AssetListStorage::tagMainDataDirty()
{
  for (AssetList &list : global_storage().values()) {
    list.tagMainDataDirty();
  }
}

void AssetListStorage::remapID(ID *id_new, ID *id_old)
{
  for (AssetList &list : global_storage().values()) {
    list.remapID(id_new, id_old);
  }
}

std::optional<eFileSelectType> AssetListStorage::asset_library_reference_to_fileselect_type(
    const AssetLibraryReference &library_reference)
{
  switch (library_reference.type) {
    case ASSET_LIBRARY_CUSTOM:
      return FILE_ASSET_LIBRARY;
    case ASSET_LIBRARY_LOCAL:
      return FILE_MAIN_ASSET;
  }

  return std::nullopt;
}

std::tuple<AssetList &, AssetListStorage::is_new_t> AssetListStorage::ensure_list_storage(
    const AssetLibraryReference &library_reference, eFileSelectType filesel_type)
{
  AssetListMap &storage = global_storage();

  if (AssetList *list = storage.lookup_ptr(library_reference)) {
    return {*list, false};
  }
  storage.add(library_reference, AssetList(filesel_type, library_reference));
  return {storage.lookup(library_reference), true};
}

/**
 * Wrapper for Construct on First Use idiom, to avoid the Static Initialization Fiasco.
 */
AssetListStorage::AssetListMap &AssetListStorage::global_storage()
{
  static AssetListMap global_storage_;
  return global_storage_;
}

/** \} */

}  // namespace blender::ed::asset

/* -------------------------------------------------------------------- */
/** \name C-API
 * \{ */

using namespace blender::ed::asset;

/**
 * Invoke asset list reading, potentially in a parallel job. Won't wait until the job is done,
 * and may return earlier.
 */
void ED_assetlist_storage_fetch(const AssetLibraryReference *library_reference, const bContext *C)
{
  AssetListStorage::fetch_library(*library_reference, *C);
}

void ED_assetlist_ensure_previews_job(const AssetLibraryReference *library_reference, bContext *C)
{

  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    list->ensurePreviewsJob(C);
  }
}

void ED_assetlist_clear(const AssetLibraryReference *library_reference, bContext *C)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    list->clear(C);
  }
}

bool ED_assetlist_storage_has_list_for_library(const AssetLibraryReference *library_reference)
{
  return AssetListStorage::lookup_list(*library_reference) != nullptr;
}

void ED_assetlist_iterate(const AssetLibraryReference *library_reference, AssetListIterFn fn)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    list->iterate(fn);
  }
}

/* TODO hack to use the File Browser path, so we can keep all the import logic handled by the asset
 * API. Get rid of this once the File Browser is integrated better with the asset list. */
static const char *assetlist_library_path_from_sfile_get_hack(const bContext *C)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  if (!sfile || !ED_fileselect_is_asset_browser(sfile)) {
    return nullptr;
  }

  FileAssetSelectParams *asset_select_params = ED_fileselect_get_asset_params(sfile);
  if (!asset_select_params) {
    return nullptr;
  }

  return filelist_dir(sfile->files);
}

std::string ED_assetlist_asset_filepath_get(const bContext *C,
                                            const AssetLibraryReference &library_reference,
                                            const AssetHandle &asset_handle)
{
  if (ED_asset_handle_get_local_id(&asset_handle) ||
      !ED_asset_handle_get_metadata(&asset_handle)) {
    return {};
  }
  const char *library_path = ED_assetlist_library_path(&library_reference);
  if (!library_path && C) {
    library_path = assetlist_library_path_from_sfile_get_hack(C);
  }
  if (!library_path) {
    return {};
  }
  const char *asset_relpath = asset_handle.file_data->relpath;

  char path[FILE_MAX_LIBEXTRA];
  BLI_join_dirfile(path, sizeof(path), library_path, asset_relpath);

  return path;
}

ImBuf *ED_assetlist_asset_image_get(const AssetHandle *asset_handle)
{
  ImBuf *imbuf = filelist_file_getimage(asset_handle->file_data);
  if (imbuf) {
    return imbuf;
  }

  return filelist_geticon_image_ex(asset_handle->file_data);
}

const char *ED_assetlist_library_path(const AssetLibraryReference *library_reference)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    return list->filepath().data();
  }
  return nullptr;
}

/**
 * \return True if the region needs a UI redraw.
 */
bool ED_assetlist_listen(const AssetLibraryReference *library_reference,
                         const wmNotifier *notifier)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    return list->listen(*notifier);
  }
  return false;
}

/**
 * \return The number of assets stored in the asset list for \a library_reference, or -1 if there
 *         is no list fetched for it.
 */
int ED_assetlist_size(const AssetLibraryReference *library_reference)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    return list->size();
  }
  return -1;
}

/**
 * Tag all asset lists in the storage that show main data as needing an update (re-fetch).
 *
 * This only tags the data. If the asset list is visible on screen, the space is still responsible
 * for ensuring the necessary redraw. It can use #ED_assetlist_listen() to check if the asset-list
 * needs a redraw for a given notifier.
 */
void ED_assetlist_storage_tag_main_data_dirty()
{
  AssetListStorage::tagMainDataDirty();
}

/**
 * Remapping of ID pointers within the asset lists. Typically called when an ID is deleted to clear
 * all references to it (\a id_new is null then).
 */
void ED_assetlist_storage_id_remap(ID *id_old, ID *id_new)
{
  AssetListStorage::remapID(id_old, id_new);
}

/**
 * Can't wait for static deallocation to run. There's nested data allocated with our guarded
 * allocator, it will complain about unfreed memory on exit.
 */
void ED_assetlist_storage_exit()
{
  AssetListStorage::destruct();
}

/** \} */
