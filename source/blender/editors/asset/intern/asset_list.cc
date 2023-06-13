/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "AS_asset_library.hh"

#include "BKE_blender_project.h"
#include "BKE_context.h"

#include "BLI_map.hh"
#include "BLI_path_util.h"
#include "BLI_utility_mixins.hh"

#include "DNA_space_types.h"

#include "WM_api.h"

/* XXX uses private header of file-space. */
#include "../space_file/file_indexer.h"
#include "../space_file/filelist.h"

#include "ED_asset_indexer.h"
#include "ED_asset_library.h"
#include "ED_asset_list.h"
#include "ED_asset_list.hh"
#include "asset_library_reference.hh"

namespace blender::ed::asset {

/* -------------------------------------------------------------------- */
/** \name Asset list API
 *
 * Internally re-uses #FileList from the File Browser. It does all the heavy lifting already.
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

  static bool listen(const wmNotifier &notifier);

  void setup();
  void fetch(const bContext &C);
  void ensurePreviewsJob(const bContext *C);
  void clear(bContext *C);

  AssetHandle asset_get_by_index(int index) const;

  bool needsRefetch() const;
  bool isLoaded() const;
  asset_system::AssetLibrary *asset_library() const;
  void iterate(AssetListHandleIterFn fn) const;
  void iterate(AssetListIterFn fn) const;
  int size() const;
  void tagMainDataDirty() const;
  void remapID(ID *id_old, ID *id_new) const;
};

AssetList::AssetList(eFileSelectType filesel_type, const AssetLibraryReference &asset_library_ref)
    : filelist_(filesel_type), library_ref_(asset_library_ref)
{
}

void AssetList::setup()
{
  FileList *files = filelist_;
  std::string asset_lib_path = AS_asset_library_root_path_from_library_ref(library_ref_);

  /* Relevant bits from file_refresh(). */
  /* TODO pass options properly. */
  filelist_setrecursion(files, FILE_SELECT_MAX_RECURSIONS);
  filelist_setsorting(files, FILE_SORT_ALPHA, false);
  filelist_setlibrary(files, &library_ref_);
  filelist_setfilter_options(
      files,
      true,
      true,
      true, /* Just always hide parent, prefer to not add an extra user option for this. */
      FILE_TYPE_BLENDERLIB,
      FILTER_ID_ALL,
      true,
      "",
      "");

  const bool use_asset_indexer = !USER_EXPERIMENTAL_TEST(&U, no_asset_indexing);
  filelist_setindexer(files, use_asset_indexer ? &file_indexer_asset : &file_indexer_noop);

  char dirpath[FILE_MAX_LIBEXTRA] = "";
  if (!asset_lib_path.empty()) {
    STRNCPY(dirpath, asset_lib_path.c_str());
  }
  filelist_setdir(files, dirpath);
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

bool AssetList::isLoaded() const
{
  return filelist_is_ready(filelist_);
}

asset_system::AssetLibrary *AssetList::asset_library() const
{
  return reinterpret_cast<asset_system::AssetLibrary *>(filelist_asset_library(filelist_));
}

void AssetList::iterate(AssetListHandleIterFn fn) const
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

void AssetList::iterate(AssetListIterFn fn) const
{
  iterate([&fn](AssetHandle handle) {
    asset_system::AssetRepresentation &asset =
        reinterpret_cast<blender::asset_system::AssetRepresentation &>(*handle.file_data->asset);

    return fn(asset);
  });
}

void AssetList::ensurePreviewsJob(const bContext *C)
{
  FileList *files = filelist_;
  int numfiles = filelist_files_ensure(files);

  filelist_cache_previews_set(files, true);
  /* TODO fetch all previews for now. */
  /* Add one extra entry to ensure nothing is lost because of integer division. */
  filelist_file_cache_slidingwindow_set(files, numfiles / 2 + 1);
  filelist_file_cache_block(files, 0);
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

AssetHandle AssetList::asset_get_by_index(int index) const
{
  return {filelist_file(filelist_, index)};
}

/**
 * \return True if the asset-list needs a UI redraw.
 */
bool AssetList::listen(const wmNotifier &notifier)
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
  global_storage().clear();
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
  switch (eAssetLibraryType(library_reference.type)) {
    case ASSET_LIBRARY_ALL:
      return FILE_ASSET_LIBRARY_ALL;
    case ASSET_LIBRARY_ESSENTIALS:
    case ASSET_LIBRARY_CUSTOM_FROM_PREFERENCES:
    case ASSET_LIBRARY_CUSTOM_FROM_PROJECT:
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

using namespace blender;
using namespace blender::ed::asset;

void ED_assetlist_storage_fetch(const AssetLibraryReference *library_reference, const bContext *C)
{
  AssetListStorage::fetch_library(*library_reference, *C);
}

bool ED_assetlist_is_loaded(const AssetLibraryReference *library_reference)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (!list) {
    return false;
  }
  if (list->needsRefetch()) {
    return false;
  }
  return list->isLoaded();
}

void ED_assetlist_ensure_previews_job(const AssetLibraryReference *library_reference,
                                      const bContext *C)
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

void ED_assetlist_iterate(const AssetLibraryReference &library_reference, AssetListHandleIterFn fn)
{
  AssetList *list = AssetListStorage::lookup_list(library_reference);
  if (list) {
    list->iterate(fn);
  }
}

void ED_assetlist_iterate(const AssetLibraryReference &library_reference, AssetListIterFn fn)
{
  AssetList *list = AssetListStorage::lookup_list(library_reference);
  if (list) {
    list->iterate(fn);
  }
}

asset_system::AssetLibrary *ED_assetlist_library_get_once_available(
    const AssetLibraryReference &library_reference)
{
  const AssetList *list = AssetListStorage::lookup_list(library_reference);
  if (!list) {
    return nullptr;
  }
  return list->asset_library();
}

AssetHandle ED_assetlist_asset_handle_get_by_index(const AssetLibraryReference *library_reference,
                                                   int asset_index)
{
  const AssetList *list = AssetListStorage::lookup_list(*library_reference);
  return list->asset_get_by_index(asset_index);
}

asset_system::AssetRepresentation *ED_assetlist_asset_get_by_index(
    const AssetLibraryReference &library_reference, int asset_index)
{
  AssetHandle asset_handle = ED_assetlist_asset_handle_get_by_index(&library_reference,
                                                                    asset_index);
  return reinterpret_cast<asset_system::AssetRepresentation *>(asset_handle.file_data->asset);
}

ImBuf *ED_assetlist_asset_image_get(const AssetHandle *asset_handle)
{
  ImBuf *imbuf = filelist_file_getimage(asset_handle->file_data);
  if (imbuf) {
    return imbuf;
  }

  return filelist_geticon_image_ex(asset_handle->file_data);
}

bool ED_assetlist_listen(const wmNotifier *notifier)
{
  return AssetList::listen(*notifier);
}

int ED_assetlist_size(const AssetLibraryReference *library_reference)
{
  AssetList *list = AssetListStorage::lookup_list(*library_reference);
  if (list) {
    return list->size();
  }
  return -1;
}

void ED_assetlist_storage_tag_main_data_dirty()
{
  AssetListStorage::tagMainDataDirty();
}

void ED_assetlist_storage_id_remap(ID *id_old, ID *id_new)
{
  AssetListStorage::remapID(id_old, id_new);
}

void ED_assetlist_storage_exit()
{
  AssetListStorage::destruct();
}

/** \} */
